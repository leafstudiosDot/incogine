// Incogine studio-preview channel protocol (plain C, no dependencies).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The game (producer) renders frames into a fixed-size shared-memory
// segment; Studio (consumer) displays them. Single instance: the segment
// name is fixed. Included by both the engine (`src/core/preview/`) and
// Studio (`src/studio/core/preview_client.*`); the runtime never depends
// on Studio code and vice versa — this header is the only shared piece.
//
// Layout (little-endian scalars, RGBA8 pixels, rows bottom-up = GL order):
//   header (128 bytes) + 1280*720*4 payload bytes.
// Frame seqlock: producer bumps frameSeq to odd while writing, then to
// even when done; the consumer copies only on even and retries when the
// two reads disagree (torn frame). Commands flow the other way: Studio
// writes cmdCode and bumps cmdSeq; the engine copies cmdSeq to ackSeq
// once handled. All counters are single-writer u32 (x86/ARM atomic by
// alignment); cross-process visibility is immediate in practice.
#ifndef ICG_PREVIEW_PROTOCOL_H
#define ICG_PREVIEW_PROTOCOL_H

#include <stdint.h>

#define ICG_PREVIEW_SHM_NAME "incogine_preview"
#define ICG_PREVIEW_MAGIC0 'I'
#define ICG_PREVIEW_MAGIC1 'C'
#define ICG_PREVIEW_MAGIC2 'G'
#define ICG_PREVIEW_MAGIC3 'P'
#define ICG_PREVIEW_MAGIC4 'R'
#define ICG_PREVIEW_MAGIC5 'E'
#define ICG_PREVIEW_MAGIC6 'V'
#define ICG_PREVIEW_MAGIC7 '3'
#define ICG_PREVIEW_VERSION 3u
#define ICG_PREVIEW_WIDTH 1280u
#define ICG_PREVIEW_HEIGHT 720u
#define ICG_PREVIEW_BPP 4u
#define ICG_PREVIEW_PITCH (ICG_PREVIEW_WIDTH * ICG_PREVIEW_BPP)
#define ICG_PREVIEW_PIXEL_BYTES (ICG_PREVIEW_PITCH * ICG_PREVIEW_HEIGHT)
#define ICG_PREVIEW_HEADER_SIZE 128u
#define ICG_PREVIEW_SHM_SIZE (ICG_PREVIEW_HEADER_SIZE + ICG_PREVIEW_PIXEL_BYTES)

#define ICG_PREVIEW_CMD_NOP 0u
#define ICG_PREVIEW_CMD_QUIT 1u
#define ICG_PREVIEW_CMD_SELECT 2u     // cmdId = object id (stored, echoed later)
#define ICG_PREVIEW_CMD_TRANSFORM 3u  // cmdId + cmdF[0..8] = pos/rot/scale xyz

typedef struct IcgPreviewHeader {
    char magic[8];            // "ICGPREV2"
    uint32_t version;         // ICG_PREVIEW_VERSION
    volatile uint32_t frameSeq;
    uint32_t width, height, pitch;
    volatile uint32_t cmdSeq;
    uint32_t cmdCode;
    volatile uint32_t ackSeq;
    uint64_t cmdId;           // object id for SELECT/TRANSFORM
    float cmdF[9];            // TRANSFORM: pos xyz, rot xyz, scale xyz
    uint64_t sessionToken;    // random per Studio launch (--studio-token);
                              // 0 = unbound (legacy); must match when set
    uint32_t reserved[9];     // pad to 128 bytes
} IcgPreviewHeader;

#endif // ICG_PREVIEW_PROTOCOL_H
