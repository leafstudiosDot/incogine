// Incogine Studio — `.incoba` asset bundle v1 (Qt-free, no third-party deps).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Goals (packaging first, NOT DRM): bundle `src/assets/` into one file,
// reduce file count/packaging overhead, add light compression later, and
// give casual resistance to accidental modification. A determined user can
// still inspect their own files; that is explicitly out of scope.
//
// Layout (little-endian):
//   magic[6] = "INCOBA", version u16 = 1, flags u16 = 0,
//   entryCount u32, then per entry:
//     pathLen u16, path bytes (UTF-8, '/' separators, relative to asset root),
//     offset u64, size u64, storedSize u64, method u8, crc32 u32,
//   then concatenated entry blobs.
// v1 only writes method 0 (stored/uncompressed); method 1+ is reserved for
// future compression. Readers must accept method 0 and reject others.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace icg {
namespace studio {
namespace incoba {

inline constexpr char kMagic[6] = {'I', 'N', 'C', 'O', 'B', 'A'};
inline constexpr uint16_t kVersion = 1;
inline constexpr uint8_t kMethodStored = 0;

struct Entry {
    std::string path;   // relative, '/' separators
    uint64_t offset = 0;
    uint64_t size = 0;        // logical size
    uint64_t storedSize = 0;  // bytes in bundle (== size for method 0)
    uint8_t method = kMethodStored;
    uint32_t crc32 = 0;
    std::vector<char> data; // populated by Reader::ReadEntry / Writer::Add
};

struct Bundle {
    std::vector<Entry> entries;
};

// IEEE CRC-32 used as a corruption check (not security).
uint32_t Crc32(const char* data, size_t size);

// Packs every regular file under `assetDir` (relative posix paths).
bool PackDirectory(const std::string& assetDir, const std::string& outFile, std::string& error);
// Writes an in-memory bundle.
bool WriteBundle(const Bundle& bundle, const std::string& outFile, std::string& error);

// ---- Split bundles + searchable index (v1) ----
//
// Large projects are split into payload-capped bundles (default 128 MiB)
// sharing one `/assets` output folder, with an `index.incobai` lookup table
// so the runtime finds any asset without opening/listing every bundle:
//
//   index magic[7] = "INCOBAI", version u16 = 1, flags u16 = 0,
//   bundleCount u16, then bundleCount x { nameLen u16, name bytes },
//   entryCount u32, then entries (sorted by path, binary-searchable):
//     pathLen u16, path bytes, bundleIdx u16,
//     offset u64 (absolute bundle file offset), size u64, crc32 u32, method u8.
//
// Bundle names are relative to the index directory. Offsets point straight
// at entry blobs, so indexed reads never parse bundle tables.
inline constexpr char kIndexMagic[7] = {'I', 'N', 'C', 'O', 'B', 'A', 'I'};
inline constexpr uint16_t kIndexVersion = 1;
inline constexpr const char* kIndexFileName = "index.incobai";
inline constexpr uint64_t kDefaultMaxBundleBytes = 128ull * 1024ull * 1024ull;

struct IndexEntry {
    std::string path;    // relative, '/' separators
    uint16_t bundle = 0; // index into Index::bundles
    uint64_t offset = 0; // absolute file offset inside the bundle
    uint64_t size = 0;
    uint32_t crc32 = 0;
    uint8_t method = kMethodStored;
};

struct Index {
    std::vector<std::string> bundles; // file names, relative to the index dir
    std::vector<IndexEntry> entries;  // sorted by path
};

bool WriteIndex(const Index& index, const std::string& outFile, std::string& error);
bool ReadIndex(const std::string& path, Index& out, std::string& error);

// Splits `assetDir` into payload-capped bundles under `outDir` plus the
// index file. One bundle keeps the legacy `<stem>.incoba` name; otherwise
// `<stem>_<NN>.incoba` (zero-padded, e.g. `a_00.incoba`). The cap is
// approximate (payload bytes; table overhead excluded). A single file
// larger than the cap gets a bundle of its own.
//
// Patch/update sets for live games are ordinary split outputs: ship the
// set (or a subset directory with its own index) to the client and mount
// it at runtime (see AssetManager::MountBundleDir) — entries there
// override the base install, which is how versioned content updates
// (e.g. gacha banners/events) are delivered without re-shipping the game.
bool PackSplit(const std::string& assetDir, const std::string& outDir,
               const std::string& stem, uint64_t maxBundleBytes, std::string& error);

class Reader {
public:
    bool Open(const std::string& path, std::string& error);
    const std::vector<Entry>& entries() const { return entries_; }
    const Entry* Find(const std::string& path) const;
    // Reads (and CRC-checks) one entry's bytes.
    bool ReadEntry(const std::string& path, std::vector<char>& out, std::string& error);
    // Extracts all entries under `destDir`.
    bool ExtractAll(const std::string& destDir, std::string& error);

private:
    std::string path_;
    std::vector<Entry> entries_;
    std::vector<char> rawBytes_;
    uint64_t dataBase_ = 0;
};

// Index-backed reader: resolves assets through `index.incobai` and reads
// only the needed blob slice (seeks to the indexed offset), so large split
// sets never get fully loaded or scanned.
class IndexedReader {
public:
    bool Open(const std::string& indexPath, std::string& error);
    const IndexEntry* Find(const std::string& path) const; // binary search
    bool ReadEntry(const std::string& path, std::vector<char>& out, std::string& error);
    const Index& index() const { return index_; }
    const std::string& baseDir() const { return baseDir_; }

private:
    Index index_;
    std::string baseDir_; // directory containing the index (bundles live here)
};

} // namespace incoba
} // namespace studio
} // namespace icg
