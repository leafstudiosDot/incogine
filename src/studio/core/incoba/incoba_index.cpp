// Incogine Studio - `.incoba` split/index packing (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Payload-capped split bundles, the searchable `index.incobai`, and
// the index-backed reader. Single-bundle codec: incoba.cpp; shared
// helpers: incoba_detail.h. Public API: incoba.h.
#include "incoba.h"
#include "incoba_detail.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace icg {
namespace studio {
namespace incoba {
bool WriteIndex(const Index& index, const std::string& outFile, std::string& error) {
    if (index.bundles.size() > 0xFFFF) {
        error = "too many bundles";
        return false;
    }
    std::vector<char> bytes;
    for (char c : kIndexMagic) {
        bytes.push_back(c);
    }
    detail::PutU16(bytes, kIndexVersion);
    detail::PutU16(bytes, 0); // flags
    detail::PutU16(bytes, static_cast<uint16_t>(index.bundles.size()));
    for (const std::string& name : index.bundles) {
        if (name.size() > 0xFFFF) {
            error = "bundle name too long: " + name;
            return false;
        }
        detail::PutU16(bytes, static_cast<uint16_t>(name.size()));
        bytes.insert(bytes.end(), name.begin(), name.end());
    }
    // Deterministic, searchable order.
    std::vector<const IndexEntry*> sorted;
    for (const IndexEntry& e : index.entries) {
        sorted.push_back(&e);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const IndexEntry* a, const IndexEntry* b) { return a->path < b->path; });
    detail::PutU32(bytes, static_cast<uint32_t>(sorted.size()));
    for (const IndexEntry* e : sorted) {
        if (e->path.size() > 0xFFFF || e->bundle >= index.bundles.size()) {
            error = "bad index entry: " + e->path;
            return false;
        }
        if (e->method != kMethodStored) {
            error = "unsupported method for path: " + e->path;
            return false;
        }
        detail::PutU16(bytes, static_cast<uint16_t>(e->path.size()));
        bytes.insert(bytes.end(), e->path.begin(), e->path.end());
        detail::PutU16(bytes, e->bundle);
        detail::PutU64(bytes, e->offset);
        detail::PutU64(bytes, e->size);
        detail::PutU32(bytes, e->crc32);
        bytes.push_back(static_cast<char>(e->method));
    }
    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot write " + outFile;
        return false;
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return true;
}

bool ReadIndex(const std::string& path, Index& out, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 13) {
        error = "file too small to be .incobai";
        return false;
    }
    const char* p = bytes.data();
    const char* end = p + bytes.size();
    if (std::memcmp(p, kIndexMagic, 7) != 0) {
        error = "bad magic (not .incobai)";
        return false;
    }
    p += 7;
    bool ok = true;
    const uint16_t version = detail::GetU16(p, end, ok);
    detail::GetU16(p, end, ok); // flags (reserved)
    const uint16_t bundleCount = detail::GetU16(p, end, ok);
    if (!ok) {
        error = "truncated index header";
        return false;
    }
    if (version != kIndexVersion) {
        error = "unsupported .incobai version";
        return false;
    }
    Index index;
    for (uint16_t i = 0; i < bundleCount; ++i) {
        const uint16_t nameLen = detail::GetU16(p, end, ok);
        if (!ok || p + nameLen > end) {
            error = "truncated bundle name";
            return false;
        }
        index.bundles.emplace_back(p, p + nameLen);
        p += nameLen;
    }
    const uint32_t entryCount = detail::GetU32(p, end, ok);
    if (!ok) {
        error = "truncated entry count";
        return false;
    }
    for (uint32_t i = 0; i < entryCount; ++i) {
        const uint16_t pathLen = detail::GetU16(p, end, ok);
        if (!ok || p + pathLen > end) {
            error = "truncated index entry path";
            return false;
        }
        IndexEntry e;
        e.path.assign(p, p + pathLen);
        p += pathLen;
        e.bundle = detail::GetU16(p, end, ok);
        e.offset = detail::GetU64(p, end, ok);
        e.size = detail::GetU64(p, end, ok);
        e.crc32 = detail::GetU32(p, end, ok);
        if (!ok || p + 1 > end) {
            error = "truncated index entry record";
            return false;
        }
        e.method = static_cast<uint8_t>(*p++);
        if (e.bundle >= index.bundles.size()) {
            error = "index entry references missing bundle: " + e.path;
            return false;
        }
        if (e.method != kMethodStored) {
            error = "unsupported index entry method for " + e.path;
            return false;
        }
        index.entries.push_back(std::move(e));
    }
    out = std::move(index);
    return true;
}

bool PackSplit(const std::string& assetDir, const std::string& outDir,
               const std::string& stem, uint64_t maxBundleBytes, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::is_directory(assetDir, ec)) {
        error = "asset dir not found: " + assetDir;
        return false;
    }
    if (maxBundleBytes == 0) {
        error = "max bundle size must be > 0";
        return false;
    }
    struct File {
        std::string rel;
        uint64_t size = 0;
    };
    std::vector<File> files;
    for (const auto& item : std::filesystem::recursive_directory_iterator(assetDir, ec)) {
        if (ec) {
            error = "directory walk failed: " + ec.message();
            return false;
        }
        if (!item.is_regular_file()) {
            continue;
        }
        std::error_code relEc;
        std::filesystem::path rel = std::filesystem::relative(item.path(), assetDir, relEc);
        if (relEc) {
            error = "cannot relativize " + item.path().string();
            return false;
        }
        std::error_code sizeEc;
        const uint64_t size = std::filesystem::file_size(item.path(), sizeEc);
        if (sizeEc) {
            error = "cannot size " + item.path().string();
            return false;
        }
        files.push_back({rel.generic_string(), size});
    }
    std::sort(files.begin(), files.end(),
              [](const File& a, const File& b) { return a.rel < b.rel; });

    // Greedy first-fit in sorted order (deterministic). An oversized single
    // file always gets a bundle of its own.
    std::vector<std::vector<File>> groups(1);
    uint64_t current = 0;
    for (const File& f : files) {
        if (!groups.back().empty() && current + f.size > maxBundleBytes) {
            groups.emplace_back();
            current = 0;
        }
        groups.back().push_back(f);
        current += f.size;
    }
    if (groups.size() == 1 && groups.back().empty()) {
        groups.clear(); // no files at all: index with zero bundles
    }

    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        error = "cannot create " + outDir + ": " + ec.message();
        return false;
    }

    const bool single = groups.size() <= 1;
    size_t width = 2;
    if (!single) {
        width = std::to_string(groups.size() - 1).size();
        if (width < 2) {
            width = 2;
        }
    }
    Index index;
    for (size_t gi = 0; gi < groups.size(); ++gi) {
        Bundle bundle;
        for (const File& f : groups[gi]) {
            Entry e;
            e.path = f.rel;
            e.method = kMethodStored;
            std::ifstream in(std::filesystem::path(assetDir) / f.rel, std::ios::binary);
            if (!in) {
                error = "cannot read " + f.rel;
                return false;
            }
            e.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            e.size = e.data.size();
            e.storedSize = e.data.size();
            e.crc32 = Crc32(e.data.data(), e.data.size());
            bundle.entries.push_back(std::move(e));
        }
        std::string name;
        if (single) {
            name = stem + ".incoba";
        } else {
            std::string num = std::to_string(gi);
            name = stem + "_" + std::string(width > num.size() ? width - num.size() : 0, '0') + num + ".incoba";
        }
        // The writer reports its exact table rows, so the index is built
        // without re-reading the bundle back through Reader.
        const std::string bundlePath = (std::filesystem::path(outDir) / name).string();
        std::vector<detail::TableEntry> rows;
        if (!detail::WriteBundleAndCollect(bundle, bundlePath, rows, error)) {
            return false;
        }
        const uint16_t bundleIdx = static_cast<uint16_t>(index.bundles.size());
        index.bundles.push_back(name);
        for (const detail::TableEntry& row : rows) {
            IndexEntry ie;
            ie.path = row.path;
            ie.bundle = bundleIdx;
            ie.offset = row.offset;
            ie.size = row.size;
            ie.crc32 = row.crc32;
            ie.method = row.method;
            index.entries.push_back(std::move(ie));
        }
    }
    return WriteIndex(index, (std::filesystem::path(outDir) / kIndexFileName).string(), error);
}

bool IndexedReader::Open(const std::string& indexPath, std::string& error) {
    if (!ReadIndex(indexPath, index_, error)) {
        return false;
    }
    baseDir_ = std::filesystem::path(indexPath).parent_path().string();
    return true;
}

const IndexEntry* IndexedReader::Find(const std::string& path) const {
    size_t lo = 0, hi = index_.entries.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (index_.entries[mid].path < path) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < index_.entries.size() && index_.entries[lo].path == path) {
        return &index_.entries[lo];
    }
    return nullptr;
}

bool IndexedReader::ReadEntry(const std::string& path, std::vector<char>& out, std::string& error) {
    const IndexEntry* e = Find(path);
    if (!e) {
        error = "entry not found: " + path;
        return false;
    }
    std::filesystem::path bundlePath = index_.bundles[e->bundle];
    if (!baseDir_.empty()) {
        bundlePath = std::filesystem::path(baseDir_) / bundlePath;
    }
    std::ifstream in(bundlePath, std::ios::binary);
    if (!in) {
        error = "cannot open bundle " + bundlePath.string();
        return false;
    }
    in.seekg(static_cast<std::streamoff>(e->offset));
    if (!in) {
        error = "cannot seek in bundle for " + path;
        return false;
    }
    out.resize(static_cast<size_t>(e->size));
    if (e->size > 0) {
        in.read(out.data(), static_cast<std::streamsize>(e->size));
        if (static_cast<uint64_t>(in.gcount()) != e->size) {
            error = "short read in bundle for " + path;
            return false;
        }
        if (Crc32(out.data(), out.size()) != e->crc32) {
            error = "crc mismatch (corrupt bundle?): " + path;
            return false;
        }
    }
    return true;
}
} // namespace incoba
} // namespace studio
} // namespace icg

