// Incogine - `.incoanim` v2 container read/write.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Every read below is bounds-checked: a truncated or corrupt file returns
// false with a message, it never reads out of bounds. Compression goes
// through the vendored miniz (Qt-free, so the game runtime uses this same
// code), one-shot mz_compress2/mz_uncompress at the default level.

#include "anim_container.h"

#include <cstring>

#include "../thirdparty/miniz/miniz.h"
#include "anim_io.h"
#include "anim_json.h"

namespace icg {
namespace anim {
namespace {

// --- little-endian codec ---

void PutU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

void PutU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

void PutU64(std::vector<uint8_t>& out, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

void PutI32(std::vector<uint8_t>& out, int32_t v) {
    PutU32(out, static_cast<uint32_t>(v));
}

void PutF32(std::vector<uint8_t>& out, float v) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(v), "float is 32 bits");
    std::memcpy(&bits, &v, sizeof(bits));
    PutU32(out, bits);
}

void PutBytes(std::vector<uint8_t>& out, const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

void PutString(std::vector<uint8_t>& out, const std::string& text) {
    PutU16(out, static_cast<uint16_t>(text.size()));
    PutBytes(out, text.data(), text.size());
}

// Bounded reader: every Get fails cleanly past the end.
class Reader {
  public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool GetU16(uint16_t& v) {
        if (!CanRead(2)) return false;
        v = static_cast<uint16_t>(data_[pos_]) |
            (static_cast<uint16_t>(data_[pos_ + 1]) << 8);
        pos_ += 2;
        return true;
    }

    bool GetU32(uint32_t& v) {
        if (!CanRead(4)) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= static_cast<uint32_t>(data_[pos_ + i]) << (8 * i);
        }
        pos_ += 4;
        return true;
    }

    bool GetU64(uint64_t& v) {
        if (!CanRead(8)) return false;
        v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(data_[pos_ + i]) << (8 * i);
        }
        pos_ += 8;
        return true;
    }

    bool GetI32(int32_t& v) {
        uint32_t u = 0;
        if (!GetU32(u)) return false;
        v = static_cast<int32_t>(u);
        return true;
    }

    bool GetF32(float& v) {
        uint32_t bits = 0;
        if (!GetU32(bits)) return false;
        std::memcpy(&v, &bits, sizeof(v));
        return true;
    }

    bool GetU8(uint8_t& v) {
        if (!CanRead(1)) return false;
        v = data_[pos_++];
        return true;
    }

    bool GetBytes(const uint8_t*& data, size_t count) {
        if (!CanRead(count)) return false;
        data = data_ + pos_;
        pos_ += count;
        return true;
    }

    bool GetString(std::string& text) {
        uint16_t len = 0;
        if (!GetU16(len)) return false;
        const uint8_t* data = nullptr;
        if (!GetBytes(data, len)) return false;
        text.assign(reinterpret_cast<const char*>(data), len);
        return true;
    }

    size_t remaining() const { return size_ - pos_; }
    size_t position() const { return pos_; }

  private:
    bool CanRead(size_t count) const { return pos_ + count <= size_; }

    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
};

// --- deflate (miniz, one-shot) ---

bool Deflate(const std::vector<uint8_t>& in, std::vector<uint8_t>& out,
             std::string& error) {
    mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(in.size()));
    out.resize(static_cast<size_t>(bound));
    mz_ulong destLen = bound;
    const int status =
        mz_compress2(out.data(), &destLen, in.data(),
                     static_cast<mz_ulong>(in.size()), MZ_DEFAULT_COMPRESSION);
    if (status != MZ_OK) {
        error = std::string("deflate failed: ") + mz_error(status);
        return false;
    }
    out.resize(static_cast<size_t>(destLen));
    return true;
}

bool Inflate(const uint8_t* data, size_t packedLen, size_t unpackedLen,
             std::vector<uint8_t>& out, std::string& error) {
    out.resize(unpackedLen);
    mz_ulong destLen = static_cast<mz_ulong>(unpackedLen);
    const int status = mz_uncompress(out.data(), &destLen, data,
                                     static_cast<mz_ulong>(packedLen));
    if (status != MZ_OK) {
        error = std::string("inflate failed (corrupt chunk?): ") +
                mz_error(status);
        return false;
    }
    if (destLen != unpackedLen) {
        error = "chunk unpacked to the wrong size (corrupt?)";
        return false;
    }
    return true;
}

// --- shared shape/keyframe codec ---

void PutColor(std::vector<uint8_t>& out, const AnimColor& color) {
    out.push_back(static_cast<uint8_t>(color.r));
    out.push_back(static_cast<uint8_t>(color.g));
    out.push_back(static_cast<uint8_t>(color.b));
    out.push_back(static_cast<uint8_t>(color.a));
}

bool GetColor(Reader& in, AnimColor& color) {
    uint8_t r = 0, g = 0, b = 0, a = 0;
    if (!in.GetU8(r) || !in.GetU8(g) || !in.GetU8(b) || !in.GetU8(a)) {
        return false;
    }
    color = AnimColor(r, g, b, a);
    return true;
}

void PutTransform(std::vector<uint8_t>& out, const AnimTransform& t) {
    PutF32(out, t.position.x);
    PutF32(out, t.position.y);
    PutF32(out, t.rotation);
    PutF32(out, t.scale.x);
    PutF32(out, t.scale.y);
    PutF32(out, t.skewX);
    PutF32(out, t.skewY);
    PutF32(out, t.alpha);
    PutColor(out, t.colorTransform);
}

bool GetTransform(Reader& in, AnimTransform& t) {
    return in.GetF32(t.position.x) && in.GetF32(t.position.y) &&
           in.GetF32(t.rotation) && in.GetF32(t.scale.x) &&
           in.GetF32(t.scale.y) && in.GetF32(t.skewX) && in.GetF32(t.skewY) &&
           in.GetF32(t.alpha) && GetColor(in, t.colorTransform);
}

void PutShape(std::vector<uint8_t>& out, const AnimShape& shape) {
    PutU64(out, shape.id);
    PutString(out, shape.name);
    const uint8_t flags = static_cast<uint8_t>(
        (shape.style.hasFill ? 1u : 0u) | (shape.style.hasStroke ? 2u : 0u));
    out.push_back(flags);
    PutColor(out, shape.style.fill);
    PutColor(out, shape.style.stroke);
    PutF32(out, shape.style.strokeWidth);
    out.push_back(static_cast<uint8_t>(shape.style.cap));
    out.push_back(static_cast<uint8_t>(shape.style.join));
    PutTransform(out, shape.transform);
    PutU32(out, static_cast<uint32_t>(shape.path.segments.size()));
    for (const AnimSegment& seg : shape.path.segments) {
        out.push_back(static_cast<uint8_t>(seg.kind));
        const int floats = seg.kind == AnimSegment::Kind::Cubic      ? 6
                           : seg.kind == AnimSegment::Kind::Close ? 0
                                                                 : 2;
        for (int i = 0; i < floats; i += 2) {
            PutF32(out, seg.p[i / 2].x);
            PutF32(out, seg.p[i / 2].y);
        }
    }
}

bool GetShape(Reader& in, AnimShape& shape) {
    if (!in.GetU64(shape.id) || !in.GetString(shape.name)) {
        return false;
    }
    uint8_t flags = 0;
    if (!in.GetU8(flags)) {
        return false;
    }
    shape.style.hasFill = (flags & 1u) != 0u;
    shape.style.hasStroke = (flags & 2u) != 0u;
    if (!GetColor(in, shape.style.fill) || !GetColor(in, shape.style.stroke) ||
        !in.GetF32(shape.style.strokeWidth)) {
        return false;
    }
    uint8_t cap = 0, join = 0;
    if (!in.GetU8(cap) || !in.GetU8(join)) {
        return false;
    }
    if (cap > 2 || join > 2) {
        return false;
    }
    shape.style.cap = static_cast<LineCap>(cap);
    shape.style.join = static_cast<LineJoin>(join);
    if (!GetTransform(in, shape.transform)) {
        return false;
    }
    uint32_t segCount = 0;
    if (!in.GetU32(segCount) || segCount > 1000000u) {
        return false; // sanity cap: no real stroke has a million segments
    }
    shape.path.segments.clear();
    shape.path.segments.reserve(segCount);
    for (uint32_t i = 0; i < segCount; ++i) {
        uint8_t kindByte = 0;
        if (!in.GetU8(kindByte) || kindByte > 3) {
            return false;
        }
        AnimSegment seg(static_cast<AnimSegment::Kind>(kindByte));
        const int floats = kindByte == 2 ? 6 : kindByte == 3 ? 0 : 2;
        for (int k = 0; k < floats; k += 2) {
            if (!in.GetF32(seg.p[k / 2].x) || !in.GetF32(seg.p[k / 2].y)) {
                return false;
            }
        }
        shape.path.segments.push_back(seg);
    }
    return true;
}

void PutTween(std::vector<uint8_t>& out, const TweenSpan& span) {
    out.push_back(static_cast<uint8_t>(span.type));
    out.push_back(static_cast<uint8_t>(span.easing.kind));
    PutF32(out, span.easing.p1x);
    PutF32(out, span.easing.p1y);
    PutF32(out, span.easing.p2x);
    PutF32(out, span.easing.p2y);
    out.push_back(span.motionFlags);
    out.push_back(span.shapeHints ? 1u : 0u);
}

bool GetTween(Reader& in, TweenSpan& span) {
    uint8_t typeByte = 0, kindByte = 0, flags = 0, hints = 0;
    if (!in.GetU8(typeByte) || typeByte > 2 || !in.GetU8(kindByte) ||
        kindByte > 4) {
        return false;
    }
    span.type = static_cast<TweenType>(typeByte);
    span.easing.kind = static_cast<EasingKind>(kindByte);
    if (!in.GetF32(span.easing.p1x) || !in.GetF32(span.easing.p1y) ||
        !in.GetF32(span.easing.p2x) || !in.GetF32(span.easing.p2y) ||
        !in.GetU8(flags) || !in.GetU8(hints) || hints > 1) {
        return false;
    }
    span.motionFlags = flags;
    span.shapeHints = hints == 1;
    return true;
}

void PutKeyframe(std::vector<uint8_t>& out, const AnimKeyframe& key) {
    PutI32(out, key.frame);
    out.push_back(key.kind == KeyframeKind::Key ? 1u : 0u);
    PutTransform(out, key.transform);
    PutTween(out, key.tweenIn);
    PutU32(out, static_cast<uint32_t>(key.shapes.size()));
    for (const AnimShape& shape : key.shapes) {
        PutShape(out, shape);
    }
}

bool GetKeyframe(Reader& in, AnimKeyframe& key) {
    int32_t frame = 0;
    uint8_t kindByte = 0;
    if (!in.GetI32(frame) || !in.GetU8(kindByte) || kindByte > 1) {
        return false;
    }
    key.frame = frame;
    key.kind = kindByte == 1 ? KeyframeKind::Key : KeyframeKind::Blank;
    if (!GetTransform(in, key.transform)) {
        return false;
    }
    if (!GetTween(in, key.tweenIn)) {
        return false;
    }
    uint32_t shapeCount = 0;
    if (!in.GetU32(shapeCount) || shapeCount > 1000000u) {
        return false;
    }
    key.shapes.clear();
    key.shapes.reserve(shapeCount);
    for (uint32_t i = 0; i < shapeCount; ++i) {
        AnimShape shape;
        if (!GetShape(in, shape)) {
            return false;
        }
        key.shapes.push_back(std::move(shape));
    }
    return true;
}

// --- chunk assembly ---

// Pointer into a vector at an offset. Used only with offsets recorded during
// the same emission, so the range is valid by construction.
const uint8_t* bytes_at(const std::vector<uint8_t>& out, size_t offset) {
    return out.data() + offset;
}

// Null-safe manifest number/bool/string reads: a hand-edited or future
// manifest may omit keys, and that must fall back, never crash.
double ManifestNum(const json::JsonValue* parent, const char* key,
                   double fallback) {
    if (parent == nullptr) {
        return fallback;
    }
    const json::JsonValue* found = parent->Find(key);
    return found != nullptr ? found->AsNumber(fallback) : fallback;
}

bool ManifestBool(const json::JsonValue* parent, const char* key,
                  bool fallback) {
    if (parent == nullptr) {
        return fallback;
    }
    const json::JsonValue* found = parent->Find(key);
    return found != nullptr ? found->AsBool(fallback) : fallback;
}

std::string ManifestString(const json::JsonValue* parent, const char* key,
                           const std::string& fallback) {
    if (parent == nullptr) {
        return fallback;
    }
    const json::JsonValue* found = parent->Find(key);
    return found != nullptr ? found->AsStringOr(fallback) : fallback;
}

struct RawChunk {
    uint32_t type = 0;
    std::vector<uint8_t> payload; // uncompressed while building
    bool compress = true;
    uint64_t layerId = 0; // index metadata (STRO only)
    int32_t frame = 0;
};

bool EmitChunk(std::vector<uint8_t>& out, const RawChunk& chunk,
               std::string& error) {
    std::vector<uint8_t> packed;
    uint32_t flags = 0;
    if (chunk.compress && !chunk.payload.empty()) {
        if (!Deflate(chunk.payload, packed, error)) {
            return false;
        }
        flags |= kChunkFlagDeflate;
    } else {
        packed = chunk.payload;
    }
    PutU32(out, chunk.type);
    PutU32(out, flags);
    PutU32(out, static_cast<uint32_t>(chunk.payload.size()));
    PutU32(out, static_cast<uint32_t>(packed.size()));
    PutBytes(out, packed.data(), packed.size());
    return true;
}

// Parsed chunk header pointing into the file (payload NOT copied).
struct ChunkRef {
    uint32_t type = 0;
    uint32_t flags = 0;
    uint32_t unpackedLen = 0;
    const uint8_t* payload = nullptr;
    uint32_t packedLen = 0;
    size_t fileOffset = 0;
};

bool ReadChunkHeader(Reader& in, uint32_t& type, uint32_t& flags,
                     uint32_t& unpackedLen, uint32_t& packedLen) {
    return in.GetU32(type) && in.GetU32(flags) && in.GetU32(unpackedLen) &&
           in.GetU32(packedLen);
}

bool InflateChunk(const ChunkRef& chunk, std::vector<uint8_t>& out,
                  std::string& error) {
    if ((chunk.flags & kChunkFlagDeflate) == 0u) {
        out.assign(chunk.payload, chunk.payload + chunk.packedLen);
        if (out.size() != chunk.unpackedLen) {
            error = "uncompressed chunk length mismatch (corrupt?)";
            return false;
        }
        return true;
    }
    return Inflate(chunk.payload, chunk.packedLen, chunk.unpackedLen, out,
                   error);
}

// Walks the whole file once: validates header/footer, collects chunk refs,
// and parses the index chunk (required: writers always emit one).
bool WalkFile(const uint8_t* bytes, size_t size, std::vector<ChunkRef>& chunks,
              std::string& error) {
    Reader in(bytes, size);
    uint8_t magic[8] = {0};
    for (int i = 0; i < 8; ++i) {
        if (!in.GetU8(magic[i])) {
            error = "file too small for a v2 container";
            return false;
        }
    }
    if (std::memcmp(magic, "INCOANIM2", 8) != 0) {
        error = "not a v2 container (use the v1 JSON loader)";
        return false;
    }
    uint16_t version = 0, flags = 0;
    uint32_t chunkCount = 0;
    if (!in.GetU16(version) || !in.GetU16(flags) || !in.GetU32(chunkCount)) {
        error = "truncated container header";
        return false;
    }
    if (version > kContainerVersion) {
        error = "container v" + std::to_string(version) +
                " is newer than this build understands (v" +
                std::to_string(kContainerVersion) + ")";
        return false;
    }
    if (chunkCount > 100000u) {
        error = "absurd chunk count (corrupt?)";
        return false;
    }
    chunks.clear();
    chunks.reserve(chunkCount);
    for (uint32_t i = 0; i < chunkCount; ++i) {
        ChunkRef ref;
        ref.fileOffset = in.position();
        if (!ReadChunkHeader(in, ref.type, ref.flags, ref.unpackedLen,
                             ref.packedLen)) {
            error = "truncated chunk header";
            return false;
        }
        if (ref.packedLen > size) {
            error = "chunk claims more bytes than the file holds (corrupt?)";
            return false;
        }
        const uint8_t* payload = nullptr;
        if (!in.GetBytes(payload, ref.packedLen)) {
            error = "truncated chunk payload";
            return false;
        }
        ref.payload = payload;
        chunks.push_back(ref);
    }
    // Footer: magic + index offset. Validated, and the index it points at must
    // be a real INDX chunk (cheap integrity check on the table lazy loading
    // depends on).
    uint8_t footerMagic[8] = {0};
    uint64_t indexOffset = 0;
    for (int i = 0; i < 8; ++i) {
        if (!in.GetU8(footerMagic[i])) {
            error = "missing container footer (truncated?)";
            return false;
        }
    }
    if (!in.GetU64(indexOffset)) {
        error = "truncated container footer";
        return false;
    }
    if (in.remaining() != 0) {
        error = "trailing bytes after the container footer (corrupt?)";
        return false;
    }
    if (std::memcmp(footerMagic, "INCOANIM$", 8) != 0) {
        error = "bad container footer magic (corrupt?)";
        return false;
    }
    bool indexOk = false;
    for (const ChunkRef& ref : chunks) {
        if (ref.type == kChunkIndex && ref.fileOffset == indexOffset) {
            indexOk = true;
        }
    }
    if (!indexOk) {
        error = "footer points at no index chunk (corrupt?)";
        return false;
    }
    return true;
}

} // namespace

bool IsContainer(const void* bytes, size_t size) {
    if (bytes == nullptr || size < 8) {
        return false;
    }
    return std::memcmp(bytes, "INCOANIM2", 8) == 0;
}

std::vector<uint8_t> SaveContainer(const AnimDocument& document) {
    using json::JsonArray;
    using json::JsonObject;
    using json::JsonValue;

    std::vector<RawChunk> chunks;

    // Manifest first: small JSON describing the project + layer TOC.
    {
        JsonObject stage;
        stage.emplace_back("width", JsonValue::Number(document.stageWidth));
        stage.emplace_back("height", JsonValue::Number(document.stageHeight));
        stage.emplace_back("fps", JsonValue::Number(document.fps));
        stage.emplace_back("background",
                           JsonValue::String(document.background.ToHex()));
        stage.emplace_back("transparentBackground",
                           JsonValue::Bool(document.transparentBackground));
        JsonObject timeline;
        timeline.emplace_back("lengthFrames",
                              JsonValue::Number(document.lengthFrames));
        timeline.emplace_back("loop", JsonValue::Bool(document.loop));
        JsonArray layers;
        for (const AnimLayer& layer : document.layers) {
            JsonObject entry;
            entry.emplace_back("id",
                               JsonValue::Number(static_cast<double>(layer.id)));
            entry.emplace_back("name", JsonValue::String(layer.name));
            entry.emplace_back("visible", JsonValue::Bool(layer.visible));
            entry.emplace_back("locked", JsonValue::Bool(layer.locked));
            entry.emplace_back("color",
                               JsonValue::String(layer.color.ToHex()));
            entry.emplace_back("outline", JsonValue::Bool(layer.outline));
            entry.emplace_back(
                "kind",
                JsonValue::Number(static_cast<double>(layer.kind)));
            JsonArray frames;
            for (const AnimKeyframe& key : layer.frames) {
                frames.emplace_back(JsonValue::Number(key.frame));
            }
            entry.emplace_back("frames", JsonValue::Array(std::move(frames)));
            layers.emplace_back(JsonValue::Object(std::move(entry)));
        }
        JsonObject root;
        root.emplace_back("containerVersion",
                          JsonValue::Number(kContainerVersion));
        root.emplace_back("stage", JsonValue::Object(std::move(stage)));
        root.emplace_back("timeline", JsonValue::Object(std::move(timeline)));
        root.emplace_back("bakeScale",
                          JsonValue::Number(static_cast<double>(document.bakeScale)));
        root.emplace_back("layers", JsonValue::Array(std::move(layers)));
        const std::string text = json::Write(JsonValue::Object(std::move(root)));
        RawChunk manifest;
        manifest.type = kChunkManifest;
        manifest.compress = false; // keep the manifest readable in a hex dump
        manifest.payload.assign(text.begin(), text.end());
        chunks.push_back(std::move(manifest));
    }

    // One stroke chunk per keyframe.
    for (const AnimLayer& layer : document.layers) {
        for (const AnimKeyframe& key : layer.frames) {
            RawChunk stroke;
            stroke.type = kChunkStroke;
            stroke.layerId = layer.id;
            stroke.frame = key.frame;
            PutU64(stroke.payload, layer.id);
            PutI32(stroke.payload, key.frame);
            PutKeyframe(stroke.payload, key);
            chunks.push_back(std::move(stroke));
        }
    }

    // (No ASET chunks: nothing embeds assets today. The type is reserved.)

    // Index last, so its offsets are final. Payloads compress at EmitChunk
    // time, so packed sizes are learned by emitting each chunk into the file
    // buffer now (recording offsets from the file start), then building the
    // index from those offsets. The header chunk count is known up front:
    // content chunks + 1 index chunk.
    std::vector<uint8_t> indexPayload;
    std::vector<size_t> chunkOffsets;
    chunkOffsets.reserve(chunks.size() + 1);
    std::vector<uint8_t> out;
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>("INCOANIM2"[i]));
    }
    PutU16(out, kContainerVersion);
    PutU16(out, 0); // flags, reserved
    PutU32(out, static_cast<uint32_t>(chunks.size() + 1)); // + index chunk
    std::string emitError;
    for (const RawChunk& chunk : chunks) {
        chunkOffsets.push_back(out.size());
        if (!EmitChunk(out, chunk, emitError)) {
            return std::vector<uint8_t>(); // cannot fail in practice (memory)
        }
    }
    // Index payload: type | offset | packedLen | layerId | frame per chunk.
    // packedLen is re-derived here from the emitted bytes (16-byte header).
    PutU32(indexPayload, static_cast<uint32_t>(chunks.size()));
    for (size_t i = 0; i < chunks.size(); ++i) {
        Reader hdr(bytes_at(out, chunkOffsets[i]), 16);
        uint32_t type = 0, cflags = 0, unpacked = 0, packed = 0;
        ReadChunkHeader(hdr, type, cflags, unpacked, packed);
        (void)unpacked;
        (void)cflags;
        PutU32(indexPayload, type);
        PutU64(indexPayload, static_cast<uint64_t>(chunkOffsets[i]));
        PutU32(indexPayload, packed);
        PutU64(indexPayload, chunks[i].layerId);
        PutI32(indexPayload, chunks[i].frame);
    }
    RawChunk indexChunk;
    indexChunk.type = kChunkIndex;
    indexChunk.payload = std::move(indexPayload);
    const size_t indexOffset = out.size();
    if (!EmitChunk(out, indexChunk, emitError)) {
        return std::vector<uint8_t>();
    }

    // Footer.
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>("INCOANIM$"[i]));
    }
    PutU64(out, static_cast<uint64_t>(indexOffset));
    return out;
}

bool LoadContainer(const void* bytes, size_t size, AnimDocument& out,
                   std::string& error) {
    if (!IsContainer(bytes, size)) {
        // v1 JSON text: the old format keeps loading unchanged.
        const std::string text(static_cast<const char*>(bytes), size);
        return Deserialize(text, out, error);
    }
    const uint8_t* data = static_cast<const uint8_t*>(bytes);
    std::vector<ChunkRef> chunks;
    if (!WalkFile(data, size, chunks, error)) {
        return false;
    }
    // Default-constructed, NOT New(): New() seeds a default "Layer 1" that
    // would survive alongside the manifest's layers. Every document field
    // comes from the manifest below; Normalize() fixes up nextId after.
    AnimDocument document;
    bool sawManifest = false;
    for (const ChunkRef& ref : chunks) {
        if (ref.type == kChunkIndex) {
            continue;
        }
        if (ref.type != kChunkManifest && ref.type != kChunkStroke &&
            ref.type != kChunkAsset) {
            continue; // forward compatibility: skip unknown chunks
        }
        std::vector<uint8_t> payload;
        if (!InflateChunk(ref, payload, error)) {
            return false;
        }
        if (ref.type == kChunkAsset) {
            continue; // nothing embeds assets yet; reserved for the future
        }
        if (ref.type == kChunkManifest) {
            const std::string text(payload.begin(), payload.end());
            json::JsonValue root;
            if (!json::Parse(text, root, error)) {
                error = "bad manifest JSON: " + error;
                return false;
            }
            // Manifest drives document-level fields; strokes arrive per chunk.
            const json::JsonValue* stage = root.Find("stage");
            document.stageWidth = static_cast<int>(
                ManifestNum(stage, "width", 1920.0));
            document.stageHeight = static_cast<int>(
                ManifestNum(stage, "height", 1080.0));
            document.fps =
                static_cast<int>(ManifestNum(stage, "fps", 24.0));
            document.background = AnimColor::FromHex(
                ManifestString(stage, "background", "#00000000"));
            document.transparentBackground =
                ManifestBool(stage, "transparentBackground", true);
            const json::JsonValue* timeline = root.Find("timeline");
            document.lengthFrames = static_cast<int>(
                ManifestNum(timeline, "lengthFrames", 1.0));
            document.loop = ManifestBool(timeline, "loop", true);
            document.bakeScale = static_cast<float>(
                ManifestNum(&root, "bakeScale", 1.0));
            if (const json::JsonValue* layers = root.Find("layers")) {
                for (const json::JsonValue& entry : layers->AsArray()) {
                    AnimLayer layer;
                    layer.id = static_cast<uint64_t>(
                        ManifestNum(&entry, "id", 0.0));
                    layer.name = ManifestString(&entry, "name", "Layer");
                    layer.visible = ManifestBool(&entry, "visible", true);
                    layer.locked = ManifestBool(&entry, "locked", false);
                    layer.color = AnimColor::FromHex(
                        ManifestString(&entry, "color", "#6E96C8FF"));
                    layer.outline = ManifestBool(&entry, "outline", false);
                    // Unknown kinds read back as Vector: old files predate
                    // the field (absent -> 0) and future kinds must not break
                    // this reader.
                    const int kindValue = static_cast<int>(
                        ManifestNum(&entry, "kind", 0.0));
                    layer.kind = (kindValue >= 0 && kindValue <= 3)
                                     ? static_cast<LayerKind>(kindValue)
                                     : LayerKind::Vector;
                    document.layers.push_back(std::move(layer));
                }
            }
            sawManifest = true;
            continue;
        }
        // STRO chunk.
        Reader in(payload.data(), payload.size());
        uint64_t layerId = 0;
        int32_t frame = 0;
        if (!in.GetU64(layerId) || !in.GetI32(frame)) {
            error = "truncated stroke chunk header";
            return false;
        }
        AnimLayer* layer = document.FindLayerById(layerId);
        if (layer == nullptr) {
            error = "stroke chunk for unknown layer";
            return false;
        }
        AnimKeyframe key;
        if (!GetKeyframe(in, key)) {
            error = "truncated stroke chunk";
            return false;
        }
        key.frame = frame; // chunk header is authoritative for placement
        if (in.remaining() != 0) {
            error = "trailing bytes in a stroke chunk (corrupt?)";
            return false;
        }
        layer->SetKeyframe(std::move(key));
    }
    if (!sawManifest) {
        error = "container has no manifest (corrupt?)";
        return false;
    }
    document.Normalize();
    out = std::move(document);
    return true;
}

bool LoadManifest(const void* bytes, size_t size, ContainerManifest& out,
                  std::string& error) {
    if (!IsContainer(bytes, size)) {
        error = "not a v2 container";
        return false;
    }
    const uint8_t* data = static_cast<const uint8_t*>(bytes);
    std::vector<ChunkRef> chunks;
    if (!WalkFile(data, size, chunks, error)) {
        return false;
    }
    for (const ChunkRef& ref : chunks) {
        if (ref.type != kChunkManifest) {
            continue;
        }
        std::vector<uint8_t> payload;
        if (!InflateChunk(ref, payload, error)) {
            return false;
        }
        const std::string text(payload.begin(), payload.end());
        json::JsonValue root;
        if (!json::Parse(text, root, error)) {
            error = "bad manifest JSON: " + error;
            return false;
        }
        ContainerManifest manifest;
        const json::JsonValue* stage = root.Find("stage");
        manifest.stageWidth =
            static_cast<int>(ManifestNum(stage, "width", 1920.0));
        manifest.stageHeight =
            static_cast<int>(ManifestNum(stage, "height", 1080.0));
        manifest.fps = static_cast<int>(ManifestNum(stage, "fps", 24.0));
        manifest.background = AnimColor::FromHex(
            ManifestString(stage, "background", "#00000000"));
        manifest.transparentBackground =
            ManifestBool(stage, "transparentBackground", true);
        const json::JsonValue* timeline = root.Find("timeline");
        manifest.lengthFrames = static_cast<int>(
            ManifestNum(timeline, "lengthFrames", 1.0));
        manifest.loop = ManifestBool(timeline, "loop", true);
        if (const json::JsonValue* layers = root.Find("layers")) {
            for (const json::JsonValue& entry : layers->AsArray()) {
                ManifestLayer layer;
                layer.id =
                    static_cast<uint64_t>(ManifestNum(&entry, "id", 0.0));
                layer.name = ManifestString(&entry, "name", "Layer");
                layer.visible = ManifestBool(&entry, "visible", true);
                layer.locked = ManifestBool(&entry, "locked", false);
                layer.color = AnimColor::FromHex(
                    ManifestString(&entry, "color", "#6E96C8FF"));
                layer.outline = ManifestBool(&entry, "outline", false);
                {
                    const int kindValue = static_cast<int>(
                        ManifestNum(&entry, "kind", 0.0));
                    layer.kind = (kindValue >= 0 && kindValue <= 3)
                                     ? static_cast<LayerKind>(kindValue)
                                     : LayerKind::Vector;
                }
                if (const json::JsonValue* frames = entry.Find("frames")) {
                    for (const json::JsonValue& f : frames->AsArray()) {
                        layer.frames.push_back(static_cast<int>(f.AsNumber(0)));
                    }
                }
                manifest.layers.push_back(std::move(layer));
            }
        }
        out = std::move(manifest);
        return true;
    }
    error = "container has no manifest (corrupt?)";
    return false;
}

bool LoadLayerFrame(const void* bytes, size_t size, uint64_t layerId, int frame,
                    AnimKeyframe& out, std::string& error) {
    if (!IsContainer(bytes, size)) {
        error = "not a v2 container";
        return false;
    }
    const uint8_t* data = static_cast<const uint8_t*>(bytes);
    // Seek via the footer + index only: no chunk payload is touched except the
    // one asked for. (WalkFile validates everything first; a corrupt file
    // fails here rather than mis-seeking.)
    std::vector<ChunkRef> chunks;
    if (!WalkFile(data, size, chunks, error)) {
        return false;
    }
    Reader footer(data + size - 16, 16);
    uint8_t magic[8] = {0};
    uint64_t indexOffset = 0;
    for (int i = 0; i < 8; ++i) {
        footer.GetU8(magic[i]);
    }
    footer.GetU64(indexOffset);
    // Find the INDX chunk at that offset and scan its entries.
    for (const ChunkRef& ref : chunks) {
        if (ref.type != kChunkIndex || ref.fileOffset != indexOffset) {
            continue;
        }
        std::vector<uint8_t> payload;
        if (!InflateChunk(ref, payload, error)) {
            return false;
        }
        Reader in(payload.data(), payload.size());
        uint32_t entryCount = 0;
        if (!in.GetU32(entryCount) || entryCount > 100000u) {
            error = "bad chunk index (corrupt?)";
            return false;
        }
        for (uint32_t i = 0; i < entryCount; ++i) {
            uint32_t type = 0, packedLen = 0;
            uint64_t offset = 0, entryLayer = 0;
            int32_t entryFrame = 0;
            if (!in.GetU32(type) || !in.GetU64(offset) || !in.GetU32(packedLen) ||
                !in.GetU64(entryLayer) || !in.GetI32(entryFrame)) {
                error = "truncated chunk index (corrupt?)";
                return false;
            }
            if (type != kChunkStroke || entryLayer != layerId ||
                entryFrame != frame) {
                continue;
            }
            if (offset + 16 + packedLen > size) {
                error = "index points outside the file (corrupt?)";
                return false;
            }
            Reader hdr(data + offset, 16);
            uint32_t hType = 0, hFlags = 0, hUnpacked = 0, hPacked = 0;
            if (!ReadChunkHeader(hdr, hType, hFlags, hUnpacked, hPacked) ||
                hType != kChunkStroke || hPacked != packedLen) {
                error = "index disagrees with the chunk header (corrupt?)";
                return false;
            }
            ChunkRef target;
            target.type = hType;
            target.flags = hFlags;
            target.unpackedLen = hUnpacked;
            target.payload = data + offset + 16;
            target.packedLen = hPacked;
            std::vector<uint8_t> stroke;
            if (!InflateChunk(target, stroke, error)) {
                return false;
            }
            Reader body(stroke.data(), stroke.size());
            uint64_t bodyLayer = 0;
            int32_t bodyFrame = 0;
            if (!body.GetU64(bodyLayer) || !body.GetI32(bodyFrame) ||
                bodyLayer != layerId || bodyFrame != frame) {
                error = "chunk disagrees with the index (corrupt?)";
                return false;
            }
            AnimKeyframe key;
            if (!GetKeyframe(body, key)) {
                error = "truncated stroke chunk";
                return false;
            }
            if (body.remaining() != 0) {
                error = "trailing bytes in a stroke chunk (corrupt?)";
                return false;
            }
            key.frame = frame;
            out = std::move(key);
            return true;
        }
        error = "no chunk for that layer and frame";
        return false;
    }
    error = "container has no index (corrupt?)";
    return false;
}

} // namespace anim
} // namespace icg
