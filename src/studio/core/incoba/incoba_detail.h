// Incogine Studio — `.incoba` shared codec internals (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Little-endian helpers plus the bundle-writer core shared by the
// single-bundle path (incoba.cpp) and the split/index path
// (incoba_index.cpp). Public API: incoba.h (unchanged).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "incoba.h"

namespace icg {
namespace studio {
namespace incoba {
namespace detail {

inline void PutU16(std::vector<char>& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

inline void PutU32(std::vector<char>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}

inline void PutU64(std::vector<char>& out, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}

inline uint16_t GetU16(const char*& p, const char* end, bool& ok) {
    if (p + 2 > end) {
        ok = false;
        return 0;
    }
    const uint16_t v =
        static_cast<uint16_t>(static_cast<uint8_t>(p[0])) |
        (static_cast<uint16_t>(static_cast<uint8_t>(p[1])) << 8);
    p += 2;
    return v;
}

inline uint32_t GetU32(const char*& p, const char* end, bool& ok) {
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

inline uint64_t GetU64(const char*& p, const char* end, bool& ok) {
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

// One collected table row (paths/offsets/sizes, no payload copy).
struct TableEntry {
    std::string path;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t crc32 = 0;
    uint8_t method = kMethodStored;
};

// Writes the bundle and reports the exact table rows written, so split
// packing can build its index without re-reading the file back.
bool WriteBundleAndCollect(const Bundle& bundle, const std::string& outFile,
                           std::vector<TableEntry>& collected, std::string& error);

} // namespace detail
} // namespace incoba
} // namespace studio
} // namespace icg
