// Incogine Studio — `.incoba` v1 implementation (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "incoba.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace icg {
namespace studio {
namespace incoba {
namespace {

void PutU16(std::vector<char>& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}
void PutU32(std::vector<char>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}
void PutU64(std::vector<char>& out, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}
uint16_t GetU16(const char*& p, const char* end, bool& ok) {
    if (p + 2 > end) {
        ok = false;
        return 0;
    }
    uint16_t v = static_cast<uint8_t>(p[0]) | (static_cast<uint16_t>(static_cast<uint8_t>(p[1])) << 8);
    p += 2;
    return v;
}
uint32_t GetU32(const char*& p, const char* end, bool& ok) {
    if (p + 4 > end) {
        ok = false;
        return 0;
    }
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<uint32_t>(static_cast<uint8_t>(p[i])) << (8 * i);
    }
    p += 4;
    return v;
}
uint64_t GetU64(const char*& p, const char* end, bool& ok) {
    if (p + 8 > end) {
        ok = false;
        return 0;
    }
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(static_cast<uint8_t>(p[i])) << (8 * i);
    }
    p += 8;
    return v;
}

} // namespace

uint32_t Crc32(const char* data, size_t size) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        ready = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ static_cast<uint8_t>(data[i])) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

bool WriteBundle(const Bundle& bundle, const std::string& outFile, std::string& error) {
    // Deterministic order keeps builds reproducible.
    std::vector<const Entry*> sorted;
    for (const Entry& e : bundle.entries) {
        sorted.push_back(&e);
    }
    std::sort(sorted.begin(), sorted.end(), [](const Entry* a, const Entry* b) { return a->path < b->path; });

    std::vector<char> header;
    for (char c : kMagic) {
        header.push_back(c);
    }
    PutU16(header, kVersion);
    PutU16(header, 0); // flags
    PutU32(header, static_cast<uint32_t>(sorted.size()));

    // Table size: per entry 2 + path + 8 + 8 + 8 + 1 + 4.
    uint64_t tableSize = 0;
    for (const Entry* e : sorted) {
        tableSize += 2 + e->path.size() + 8 + 8 + 8 + 1 + 4;
    }
    const uint64_t dataBase = header.size() + tableSize;

    std::vector<char> table;
    uint64_t offset = dataBase;
    for (const Entry* e : sorted) {
        if (e->path.size() > 0xFFFF) {
            error = "path too long: " + e->path;
            return false;
        }
        if (e->method != kMethodStored) {
            error = "unsupported method for path: " + e->path;
            return false;
        }
        PutU16(table, static_cast<uint16_t>(e->path.size()));
        table.insert(table.end(), e->path.begin(), e->path.end());
        PutU64(table, offset);
        PutU64(table, static_cast<uint64_t>(e->data.size()));
        PutU64(table, static_cast<uint64_t>(e->data.size()));
        table.push_back(static_cast<char>(e->method));
        PutU32(table, Crc32(e->data.data(), e->data.size()));
        offset += e->data.size();
    }

    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot write " + outFile;
        return false;
    }
    out.write(header.data(), static_cast<std::streamsize>(header.size()));
    out.write(table.data(), static_cast<std::streamsize>(table.size()));
    for (const Entry* e : sorted) {
        if (!e->data.empty()) {
            out.write(e->data.data(), static_cast<std::streamsize>(e->data.size()));
        }
    }
    return true;
}

bool PackDirectory(const std::string& assetDir, const std::string& outFile, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::is_directory(assetDir, ec)) {
        error = "asset dir not found: " + assetDir;
        return false;
    }
    Bundle bundle;
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
        Entry e;
        e.path = rel.generic_string();
        e.method = kMethodStored;
        std::ifstream in(item.path(), std::ios::binary);
        if (!in) {
            error = "cannot read " + item.path().string();
            return false;
        }
        e.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        e.size = e.data.size();
        e.storedSize = e.data.size();
        e.crc32 = Crc32(e.data.data(), e.data.size());
        bundle.entries.push_back(std::move(e));
    }
    return WriteBundle(bundle, outFile, error);
}

bool Reader::Open(const std::string& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12) {
        error = "file too small to be .incoba";
        return false;
    }
    const char* p = bytes.data();
    const char* end = p + bytes.size();
    if (std::memcmp(p, kMagic, 6) != 0) {
        error = "bad magic (not .incoba)";
        return false;
    }
    p += 6;
    bool ok = true;
    const uint16_t version = GetU16(p, end, ok);
    GetU16(p, end, ok); // flags (reserved)
    const uint32_t count = GetU32(p, end, ok);
    if (!ok) {
        error = "truncated header";
        return false;
    }
    if (version != kVersion) {
        error = "unsupported .incoba version";
        return false;
    }
    std::vector<Entry> entries;
    for (uint32_t i = 0; i < count; ++i) {
        const uint16_t pathLen = GetU16(p, end, ok);
        if (!ok || p + pathLen > end) {
            error = "truncated entry path";
            return false;
        }
        Entry e;
        e.path.assign(p, p + pathLen);
        p += pathLen;
        e.offset = GetU64(p, end, ok);
        e.size = GetU64(p, end, ok);
        e.storedSize = GetU64(p, end, ok);
        if (!ok || p + 1 + 4 > end) {
            error = "truncated entry record";
            return false;
        }
        e.method = static_cast<uint8_t>(*p++);
        uint32_t crc = 0;
        std::memcpy(&crc, p, 4);
        // Little-endian decode.
        e.crc32 = static_cast<uint32_t>(static_cast<uint8_t>(p[0])) |
                  (static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 8) |
                  (static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 16) |
                  (static_cast<uint32_t>(static_cast<uint8_t>(p[3])) << 24);
        (void)crc;
        p += 4;
        if (e.method != kMethodStored) {
            error = "unsupported entry method for " + e.path;
            return false;
        }
        entries.push_back(e);
    }
    dataBase_ = static_cast<uint64_t>(p - bytes.data());
    // Validate offsets fit inside the file.
    for (const Entry& e : entries) {
        if (e.offset < dataBase_ || e.offset + e.storedSize > bytes.size()) {
            error = "entry out of range: " + e.path;
            return false;
        }
    }
    path_ = path;
    entries_ = std::move(entries);
    rawBytes_ = std::move(bytes);
    return true;
}

const Entry* Reader::Find(const std::string& path) const {
    for (const Entry& e : entries_) {
        if (e.path == path) {
            return &e;
        }
    }
    return nullptr;
}

bool Reader::ReadEntry(const std::string& path, std::vector<char>& out, std::string& error) {
    const Entry* e = Find(path);
    if (!e) {
        error = "entry not found: " + path;
        return false;
    }
    if (rawBytes_.empty()) {
        std::ifstream in(path_, std::ios::binary);
        if (!in) {
            error = "cannot reopen " + path_;
            return false;
        }
        rawBytes_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    if (e->offset + e->storedSize > rawBytes_.size()) {
        error = "entry out of range: " + path;
        return false;
    }
    out.assign(rawBytes_.data() + e->offset, rawBytes_.data() + e->offset + e->storedSize);
    if (out.size() != e->size || Crc32(out.data(), out.size()) != e->crc32) {
        error = "crc mismatch (corrupt bundle?): " + path;
        return false;
    }
    return true;
}

bool Reader::ExtractAll(const std::string& destDir, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);
    for (const Entry& e : entries_) {
        std::vector<char> data;
        if (!ReadEntry(e.path, data, error)) {
            return false;
        }
        std::filesystem::path dest = std::filesystem::path(destDir) / e.path;
        std::filesystem::create_directories(dest.parent_path(), ec);
        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "cannot write " + dest.string();
            return false;
        }
        if (!data.empty()) {
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
        }
    }
    return true;
}

bool WriteIndex(const Index& index, const std::string& outFile, std::string& error) {
    if (index.bundles.size() > 0xFFFF) {
        error = "too many bundles";
        return false;
    }
    std::vector<char> bytes;
    for (char c : kIndexMagic) {
        bytes.push_back(c);
    }
    PutU16(bytes, kIndexVersion);
    PutU16(bytes, 0); // flags
    PutU16(bytes, static_cast<uint16_t>(index.bundles.size()));
    for (const std::string& name : index.bundles) {
        if (name.size() > 0xFFFF) {
            error = "bundle name too long: " + name;
            return false;
        }
        PutU16(bytes, static_cast<uint16_t>(name.size()));
        bytes.insert(bytes.end(), name.begin(), name.end());
    }
    // Deterministic, searchable order.
    std::vector<const IndexEntry*> sorted;
    for (const IndexEntry& e : index.entries) {
        sorted.push_back(&e);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const IndexEntry* a, const IndexEntry* b) { return a->path < b->path; });
    PutU32(bytes, static_cast<uint32_t>(sorted.size()));
    for (const IndexEntry* e : sorted) {
        if (e->path.size() > 0xFFFF || e->bundle >= index.bundles.size()) {
            error = "bad index entry: " + e->path;
            return false;
        }
        if (e->method != kMethodStored) {
            error = "unsupported method for path: " + e->path;
            return false;
        }
        PutU16(bytes, static_cast<uint16_t>(e->path.size()));
        bytes.insert(bytes.end(), e->path.begin(), e->path.end());
        PutU16(bytes, e->bundle);
        PutU64(bytes, e->offset);
        PutU64(bytes, e->size);
        PutU32(bytes, e->crc32);
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
    const uint16_t version = GetU16(p, end, ok);
    GetU16(p, end, ok); // flags (reserved)
    const uint16_t bundleCount = GetU16(p, end, ok);
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
        const uint16_t nameLen = GetU16(p, end, ok);
        if (!ok || p + nameLen > end) {
            error = "truncated bundle name";
            return false;
        }
        index.bundles.emplace_back(p, p + nameLen);
        p += nameLen;
    }
    const uint32_t entryCount = GetU32(p, end, ok);
    if (!ok) {
        error = "truncated entry count";
        return false;
    }
    for (uint32_t i = 0; i < entryCount; ++i) {
        const uint16_t pathLen = GetU16(p, end, ok);
        if (!ok || p + pathLen > end) {
            error = "truncated index entry path";
            return false;
        }
        IndexEntry e;
        e.path.assign(p, p + pathLen);
        p += pathLen;
        e.bundle = GetU16(p, end, ok);
        e.offset = GetU64(p, end, ok);
        e.size = GetU64(p, end, ok);
        e.crc32 = GetU32(p, end, ok);
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
        // Offsets must match the writer: recompute by writing, then reread
        // the table back through Reader for exact offsets.
        const std::string bundlePath = (std::filesystem::path(outDir) / name).string();
        if (!WriteBundle(bundle, bundlePath, error)) {
            return false;
        }
        Reader verify;
        if (!verify.Open(bundlePath, error)) {
            return false;
        }
        const uint16_t bundleIdx = static_cast<uint16_t>(index.bundles.size());
        index.bundles.push_back(name);
        for (const Entry& ve : verify.entries()) {
            IndexEntry ie;
            ie.path = ve.path;
            ie.bundle = bundleIdx;
            ie.offset = ve.offset;
            ie.size = ve.size;
            ie.crc32 = ve.crc32;
            ie.method = ve.method;
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
