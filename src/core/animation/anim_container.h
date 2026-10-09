// Incogine - `.incoanim` v2 container format.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// v1 was a single pretty-printed JSON document. It stored every tessellated
// segment as its own `{"k","p"}` object, so files grew with DERIVED geometry
// instead of source geometry (measured: 1.28 MB for one 30px brush stroke).
// v2 keeps the `.incoanim` extension but changes the bytes: a chunked binary
// container with a JSON manifest, per-layer/per-frame binary stroke chunks,
// DEFLATE-compressed payloads (vendored miniz, Qt-free so the game runtime can
// read it), and an index for lazy loading.
//
// Layout, all integers little-endian:
//
//   header:  "INCOANIM2" (8B) | u16 containerVersion=2 | u16 flags=0 |
//            u32 chunkCount
//   chunk:   u32 type | u32 flags (bit0 = DEFLATE) | u32 unpackedLen |
//            u32 packedLen | payload[packedLen]
//   footer:  "INCOANIM$" (8B) | u64 indexChunkOffset (from file start)
//
// Chunk types (fourCC):
//   'MHDR' manifest.json (UTF-8): version, stage, fps, timeline, and the layer
//          table of contents. Small and human-readable by design.
//   'STRO' one layer's keyframe: u64 layerId | i32 frame | u8 keyKind |
//          keyframe transform | u32 shapeCount | shapes. A shape is:
//          u64 id | u16 nameLen | name bytes | u8 styleFlags (bit0 fill,
//          bit1 stroke) | fillRGBA | strokeRGBA | f32 width | u8 cap | u8 join
//          | shape transform | u32 segCount | segments, where a segment is
//          u8 kind (0 Move, 1 Line, 2 Cubic, 3 Close) followed by its floats
//          (M/L: 2, C: 6, Z: 0), all f32.
//   'ASET' embedded asset: u16 pathLen | path bytes | u64 dataLen | data.
//          Nothing embeds assets today; the type exists so the format never
//          needs a version bump just to carry one.
//   'INDX' index: u32 entryCount, then per entry u32 type | u64 offset |
//          u32 packedLen | u64 layerId | i32 frame. layerId/frame are only
//          meaningful for STRO chunks; they let lazy loading seek straight to
//          a frame without touching anything else.
//
// Forward compatibility: unknown chunk types are SKIPPED, never an error.
// Old files: anything not starting with the v2 magic loads through the v1
// JSON path, and the next save writes v2 (migrate on save).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "anim_document.h"

namespace icg {
namespace anim {

// Container version written by SaveContainer. Old readers reject anything
// newer with a clear message rather than mis-reading it.
inline constexpr uint16_t kContainerVersion = 2;

inline constexpr char kContainerMagic[8] = {'I', 'N', 'C', 'O',
                                            'A', 'N', 'I', 'M'};
inline constexpr char kContainerTag = '2';
inline constexpr char kContainerFooterMagic[8] = {'I', 'N', 'C', 'O',
                                                  'A', 'N', 'I', 'M'};

// Chunk types (little-endian fourCC as read from the file).
inline constexpr uint32_t kChunkManifest = 0x5244484Du; // 'MHDR'
inline constexpr uint32_t kChunkStroke = 0x4F525453u;   // 'STRO'
inline constexpr uint32_t kChunkAsset = 0x54455341u;    // 'ASET'
inline constexpr uint32_t kChunkIndex = 0x58444E49u;    // 'INDX'

inline constexpr uint32_t kChunkFlagDeflate = 1u << 0;

// True when `bytes` starts with the v2 magic. Anything else is treated as v1
// JSON text (which is how old files keep loading).
bool IsContainer(const void* bytes, size_t size);

// Serializes `document` as v2 container bytes.
std::vector<uint8_t> SaveContainer(const AnimDocument& document);

// Parses v2 container bytes OR v1 JSON text (auto-detected) into `out`.
// Returns false with a human-readable `error` on malformed input, an
// unsupported container version, or a document that fails validation.
// Truncated/corrupt input is always an error, never a crash: every read is
// bounds-checked.
bool LoadContainer(const void* bytes, size_t size, AnimDocument& out,
                   std::string& error);

// One layer's table-of-contents entry from the manifest: which frames hold
// keyframes, so a browser or the timeline can seek without parsing strokes.
struct ManifestLayer {
    uint64_t id = 0;
    std::string name;
    bool visible = true;
    bool locked = false;
    AnimColor color = AnimColor(110, 150, 200, 255);
    bool outline = false;
    LayerKind kind = LayerKind::Vector;
    std::vector<int> frames;
};

struct ContainerManifest {
    int stageWidth = 1920;
    int stageHeight = 1080;
    int fps = 24;
    AnimColor background = AnimColor(0, 0, 0, 0);
    bool transparentBackground = true;
    int lengthFrames = 1;
    bool loop = true;
    std::vector<ManifestLayer> layers;
};

// Reads only the header + manifest + index: enough to list a project's
// contents without touching any stroke data (fast open for large projects).
bool LoadManifest(const void* bytes, size_t size, ContainerManifest& out,
                  std::string& error);

// Lazy frame load: seeks the index straight to one (layer, frame) chunk and
// parses only it. Check the manifest for existence first: asking for a frame
// with no chunk is a "not found" error, corrupt bytes are a "corrupt" error.
bool LoadLayerFrame(const void* bytes, size_t size, uint64_t layerId, int frame,
                    AnimKeyframe& out, std::string& error);

} // namespace anim
} // namespace icg
