// Incogine Animator - RAM frame cache (two-tier: layer images + composites).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Playback/scrub smoothness: re-rasterizing every vector subpath per frame
// drops frames on dense brushwork, so baked frames are kept as QImages at
// Final preview quality (the canonical quality export bakes at, so cached
// pixels, scrubbing, and exported pixels all agree regardless of the viewport
// setting). One bake path serves both tiers (painthelp.h), so the cache can
// never drift from the canvas.
//
// Two tiers because they invalidate differently:
//   * layer images survive compositing-only edits (reorder, visibility): only
//     assembled frames drop, and the next composite reassembles from intact
//     layer pixels without re-rasterizing a single vector.
//   * content edits drop exactly their (layer, frame-span) footprint, resolved
//     live from the command's rasterRange - never the whole project.
// The stage background composites live underneath (never baked), so background
// color edits touch no cached pixels at all.
//
// Threading: prefetch bakes run on a worker thread from a snapshot copy of the
// document taken on the GUI thread. The worker touches nothing shared: results
// come back over a queued signal, and a generation counter (bumped on every
// invalidation) drops results for edits that landed mid-bake. All maps live on
// the GUI thread only. Synchronous bakes (cache miss during scrub) run on the
// GUI thread with the same static bake function - correctness first, the worker
// makes the miss rare during playback, not impossible.
//
// Eviction: LRU by last access across both tiers under one byte budget
// (default 256 MB; a 1920x1080 composite is ~8 MB). Frames near the playhead
// ([playhead-2, playhead+8]) are evicted last, so playback never throws away
// the frame it is about to show.
//
// ASCII-only by repo convention.
#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QThread>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

#include "animation/anim_document.h"

class AnimatorDocument;

// Bake job for the worker thread. The snapshot is a full document copy owned
// by the job, so the worker never touches the live document.
struct FrameCacheJob {
    quint64 generation = 0;
    int frame = 1;
    std::shared_ptr<const icg::anim::AnimDocument> snapshot;
};

Q_DECLARE_METATYPE(FrameCacheJob)

// One baked layer image inside a worker result.
struct FrameCacheLayerResult {
    uint64_t layerId = 0;
    QImage image;
};

Q_DECLARE_METATYPE(FrameCacheLayerResult)

// Worker result: the assembled composite plus every layer image rasterized
// along the way (feeds the layer tier as a side effect).
struct FrameCacheResult {
    quint64 generation = 0;
    int frame = 1;
    int stageWidth = 0;
    int stageHeight = 0;
    QImage composite;
    std::vector<FrameCacheLayerResult> layers;
};

Q_DECLARE_METATYPE(FrameCacheResult)

class FrameCacheWorker : public QObject {
    Q_OBJECT

public:
    explicit FrameCacheWorker(QObject* parent = nullptr);

public slots:
    void bake(FrameCacheJob job);

signals:
    void baked(FrameCacheResult result);
};

class FrameCache : public QObject {
    Q_OBJECT

public:
    struct LayerKey {
        uint64_t layerId = 0;
        int frame = 1;
        bool operator==(const LayerKey& other) const {
            return layerId == other.layerId && frame == other.frame;
        }
    };

    explicit FrameCache(QObject* parent = nullptr);
    ~FrameCache() override;

    // Live document this cache bakes from (non-owning). Synchronous bakes read
    // it directly; prefetch copies it into job snapshots.
    void setDocument(AnimatorDocument* document);

    // --- budget + playhead ---
    void setBudgetBytes(qint64 bytes);
    qint64 budgetBytes() const { return budgetBytes_; }
    qint64 bytesUsed() const { return bytesUsed_; }
    // Protection window for eviction ([frame-2, frame+8] evicted last).
    void setPlayhead(int frame);
    int playhead() const { return playhead_; }

    // --- queries (all GUI thread) ---
    bool hasComposite(int frame) const;
    bool hasLayer(uint64_t layerId, int frame) const;
    bool isCached(int frame) const { return hasComposite(frame); }
    // Composite without baking (null on a miss): the paint path's lookup, so
    // a repaint never triggers a synchronous bake.
    QImage cachedComposite(int frame) const;
    // Frames holding a composite, for the timeline cache strip.
    QVector<int> cachedFrames() const;

    // --- baking (GUI thread, synchronous) ---
    // Returns the composite, baking and storing it on a miss. Never null for
    // a valid document (empty frames yield a transparent image); null only
    // when no document is set.
    QImage composite(int frame);
    // Single layer image (transparent when the layer contributes nothing).
    QImage layerImage(uint64_t layerId, int frame);

    // Queue background bakes around the playhead from one shared snapshot.
    // Missing composites in [playhead-2, playhead+8] go to the worker; already
    // cached frames are skipped without copying anything.
    void prefetchAround(int playhead);

    // Stats for tests/diagnostics.
    quint64 hits() const { return hits_; }
    quint64 misses() const { return misses_; }
    quint64 syncBakes() const { return syncBakes_; }

    // Pure bakes (no cache access): safe on any thread given a snapshot.
    // Final quality: kFlattenTolerance at 1x (qualitySpec(Final)).
    static QImage bakeLayerImage(const icg::anim::AnimDocument& snapshot,
                                 uint64_t layerId, int frame, float tolerance);
    static FrameCacheResult bakeComposite(
        const icg::anim::AnimDocument& snapshot, int frame, quint64 generation,
        float tolerance);

public slots:
    // Precise invalidation from AnimatorDocument::documentEdited: drops cached
    // pixels in [firstFrame, lastFrame] (layerId 0 = all layers; first > last
    // = none). compositeOnly (reorder, visibility) drops assembled frames but
    // keeps layer pixels, so the next composite reassembles without vector
    // work. Bumps the generation: worker results in flight are dropped.
    void invalidate(uint64_t layerId, int firstFrame, int lastFrame,
                    bool compositeOnly);
    void clear();

signals:
    // A worker bake landed and was stored (or a sync store happened).
    void cacheChanged();
    void frameReady(int frame);

private slots:
    void onWorkerBaked(const FrameCacheResult& result);

private:
    struct Entry {
        QImage image;
        quint64 lastAccess = 0;
    };

    // Drops everything when the stage size moved (entries are stage-sized).
    // Returns false when no document is set.
    bool checkStage();
    void touch(quint64& stamp) { stamp = ++clock_; }
    void storeComposite(int frame, const QImage& image);
    void storeLayer(uint64_t layerId, int frame, const QImage& image);
    void evictToFit();
    bool isProtected(int frame) const;
    // Assembles a composite from intact layer images. Returns null when any
    // contributing layer image is missing (caller falls back to a full bake).
    QImage assembleFromLayers(int frame);

    AnimatorDocument* document_ = nullptr;

    QHash<int, Entry> composites_;
    QHash<LayerKey, Entry> layers_;
    qint64 budgetBytes_ = 256LL * 1024LL * 1024LL;
    qint64 bytesUsed_ = 0;
    quint64 clock_ = 0;
    quint64 generation_ = 0;
    int playhead_ = 1;
    int stageWidth_ = 0;
    int stageHeight_ = 0;

    quint64 hits_ = 0;
    quint64 misses_ = 0;
    quint64 syncBakes_ = 0;
    int pendingJobs_ = 0;

    QThread workerThread_;
    FrameCacheWorker* worker_ = nullptr;
};

inline uint qHash(const FrameCache::LayerKey& key, uint seed = 0) {
    return qHash(key.layerId, seed) ^ (static_cast<uint>(key.frame) + seed);
}
