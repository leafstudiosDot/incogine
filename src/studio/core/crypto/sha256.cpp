// Incogine Studio — SHA-256 implementation (FIPS 180-4, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "sha256.h"

#include <cstdio>
#include <cstring>

namespace icg {
namespace studio {
namespace sha256 {
namespace {

constexpr uint32_t kK[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

uint32_t RotR(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

struct Context {
    uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                     0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint64_t total = 0; // bytes absorbed
    uint8_t block[64] = {};
    size_t used = 0;
};

void Compress(Context& ctx, const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = ctx.h[0], b = ctx.h[1], c = ctx.h[2], d = ctx.h[3];
    uint32_t e = ctx.h[4], f = ctx.h[5], g = ctx.h[6], h = ctx.h[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = h + s1 + ch + kK[i] + w[i];
        const uint32_t s0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    ctx.h[0] += a;
    ctx.h[1] += b;
    ctx.h[2] += c;
    ctx.h[3] += d;
    ctx.h[4] += e;
    ctx.h[5] += f;
    ctx.h[6] += g;
    ctx.h[7] += h;
}

void Absorb(Context& ctx, const uint8_t* data, size_t size) {
    ctx.total += size;
    while (size > 0) {
        const size_t room = 64 - ctx.used;
        const size_t take = size < room ? size : room;
        std::memcpy(ctx.block + ctx.used, data, take);
        ctx.used += take;
        data += take;
        size -= take;
        if (ctx.used == 64) {
            Compress(ctx, ctx.block);
            ctx.used = 0;
        }
    }
}

void Finalize(Context& ctx, uint8_t digest[32]) {
    const uint64_t bitLen = ctx.total * 8;
    uint8_t one = 0x80;
    Absorb(ctx, &one, 1);
    uint8_t zero = 0;
    while (ctx.used != 56) {
        Absorb(ctx, &zero, 1);
    }
    uint8_t lenBytes[8];
    for (int i = 0; i < 8; ++i) {
        lenBytes[i] = static_cast<uint8_t>(bitLen >> (56 - 8 * i));
    }
    Absorb(ctx, lenBytes, 8);
    for (int i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<uint8_t>(ctx.h[i] >> 24);
        digest[i * 4 + 1] = static_cast<uint8_t>(ctx.h[i] >> 16);
        digest[i * 4 + 2] = static_cast<uint8_t>(ctx.h[i] >> 8);
        digest[i * 4 + 3] = static_cast<uint8_t>(ctx.h[i]);
    }
}

} // namespace

std::string HexOf(const void* data, size_t size) {
    Context ctx;
    Absorb(ctx, static_cast<const uint8_t*>(data), size);
    uint8_t digest[32];
    Finalize(ctx, digest);
    constexpr char kHex[] = "0123456789abcdef";
    char hex[64];
    for (int i = 0; i < 32; ++i) {
        hex[i * 2] = kHex[digest[i] >> 4];
        hex[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    return std::string(hex, 64);
}

std::string HexOf(const std::vector<uint8_t>& data) {
    return data.empty() ? HexOf(nullptr, 0) : HexOf(data.data(), data.size());
}

bool HexOfFile(const std::string& path, std::string& outHex, std::string& error) {
    FILE* file = nullptr;
#ifdef _WIN32
    if (fopen_s(&file, path.c_str(), "rb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(path.c_str(), "rb");
#endif
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    Context ctx;
    uint8_t buf[65536];
    size_t got = 0;
    while ((got = std::fread(buf, 1, sizeof(buf), file)) > 0) {
        Absorb(ctx, buf, got);
    }
    const bool readOk = std::ferror(file) == 0;
    std::fclose(file);
    if (!readOk) {
        error = "cannot read " + path;
        return false;
    }
    uint8_t digest[32];
    Finalize(ctx, digest);
    constexpr char kHex[] = "0123456789abcdef";
    char hex[64];
    for (int i = 0; i < 32; ++i) {
        hex[i * 2] = kHex[digest[i] >> 4];
        hex[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    outHex.assign(hex, 64);
    return true;
}

} // namespace sha256
} // namespace studio
} // namespace icg
