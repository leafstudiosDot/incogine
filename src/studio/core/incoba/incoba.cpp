// Incogine Studio - `.incoba` v1 bundle core (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// CRC, single-bundle writer, directory packer, and the bundle Reader.
// Split/index packing lives in incoba_index.cpp; shared codec helpers
// in incoba_detail.h. Public API: incoba.h.
#include "incoba.h"
#include "incoba_detail.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace icg {
namespace studio {
namespace incoba {
uint32_t Crc32(const char* data, size_t size) {
    struct Table {
        uint32_t t[256];
        Table() {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                t[i] = c;
            }
        }
    };
    static const Table table; // function-local: thread-safe one-time init
    if (size == 0) {
        return 0;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc = table.t[(crc ^ static_cast<uint8_t>(data[i])) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
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
    const uint16_t version = detail::GetU16(p, end, ok);
    detail::GetU16(p, end, ok); // flags (reserved)
    const uint32_t count = detail::GetU32(p, end, ok);
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
        const uint16_t pathLen = detail::GetU16(p, end, ok);
        if (!ok || p + pathLen > end) {
            error = "truncated entry path";
            return false;
        }
        Entry e;
        e.path.assign(p, p + pathLen);
        p += pathLen;
        e.offset = detail::GetU64(p, end, ok);
        e.size = detail::GetU64(p, end, ok);
        e.storedSize = detail::GetU64(p, end, ok);
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

namespace detail {

bool WriteBundleAndCollect(const Bundle& bundle, const std::string& outFile,
                           std::vector<TableEntry>& collected, std::string& error) {
    collected.clear();
    collected.reserve(bundle.entries.size());
    // Deterministic order keeps builds reproducible.
    std::vector<const Entry*> sorted;
    sorted.reserve(bundle.entries.size());
    for (const Entry& e : bundle.entries) {
        sorted.push_back(&e);
    }
    std::sort(sorted.begin(), sorted.end(), [](const Entry* a, const Entry* b) { return a->path < b->path; });

    std::vector<char> header;
    header.reserve(14); // magic[6] + version u16 + flags u16 + count u32
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
    table.reserve(static_cast<size_t>(tableSize));
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
        const uint32_t crc = Crc32(e->data.data(), e->data.size());
        PutU32(table, crc);
        collected.push_back(TableEntry{e->path, offset,
                                       static_cast<uint64_t>(e->data.size()),
                                       crc, e->method});
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

} // namespace detail

bool WriteBundle(const Bundle& bundle, const std::string& outFile, std::string& error) {
    std::vector<detail::TableEntry> ignored;
    return detail::WriteBundleAndCollect(bundle, outFile, ignored, error);
}
} // namespace incoba
} // namespace studio
} // namespace icg

