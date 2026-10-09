// Incogine Animator - RAM frame cache implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
// ASCII-only by repo convention.

#include "framecache.h"

#include <QPainter>
#include <QMetaObject>

#include <algorithm>

#include "animation/anim_geometry.h"
#include "document.h"
#include "painthelp.h"

namespace {

// Final preview quality bakes at 1x the base flatten tolerance (see
// qualitySpec(Final) in canvas.h). Spelled out here so the cache never depends
// on a widget header for one float.
constexpr float kCacheTolerance = icg::anim::kFlattenTolerance;

// Prefetch window around the playhead: two frames back (short scrub-back),
// eight forward (playback direction). Matches the eviction protection.
constexpr int kPrefetchBack = 2;
constexpr int kPrefetchForward = 8;
// Max worker jobs in flight: bounds snapshot copies during fast scrubbing.
// Stale-frame bakes still land usefully (LRU + timeline strip), they just
// stop multiplying when the worker is busy.
constexpr int kMaxPendingJobs = 6;

} // namespace

FrameCacheWorker::FrameCacheWorker(QObject* parent) : QObject(parent) {}

void FrameCacheWorker::bake(FrameCacheJob job) {
    if (job.snapshot == nullptr) {
        return;
    }
    FrameCacheResult result =
        FrameCache::bakeComposite(*job.snapshot, job.frame, job.generation,
                                  kCacheTolerance);
    emit baked(result);
}

FrameCache::FrameCache(QObject* parent) : QObject(parent) {
    qRegisterMetaType<FrameCacheJob>("FrameCacheJob");
    qRegisterMetaType<FrameCacheResult>("FrameCacheResult");
    worker_ = new FrameCacheWorker();
    worker_->moveToThread(&workerThread_);
    connect(&workerThread_, &QThread::finished, worker_,
            &QObject::deleteLater);
    connect(worker_, &FrameCacheWorker::baked, this,
            &FrameCache::onWorkerBaked);
    workerThread_.setObjectName(QStringLiteral("FrameCacheWorker"));
    workerThread_.start();
}

FrameCache::~FrameCache() {
    workerThread_.quit();
    workerThread_.wait();
}

void FrameCache::setDocument(AnimatorDocument* document) {
    document_ = document;
    clear();
}

void FrameCache::setBudgetBytes(qint64 bytes) {
    budgetBytes_ = std::max<qint64>(1LL * 1024LL * 1024LL, bytes);
    evictToFit();
}

void FrameCache::setPlayhead(int frame) {
    playhead_ = std::max(1, frame);
}

bool FrameCache::hasComposite(int frame) const {
    return composites_.contains(frame);
}

bool FrameCache::hasLayer(uint64_t layerId, int frame) const {
    return layers_.contains(LayerKey{layerId, frame});
}

QImage FrameCache::cachedComposite(int frame) const {
    auto hit = composites_.find(frame);
    if (hit != composites_.end()) {
        return hit->image;
    }
    return QImage();
}

QVector<int> FrameCache::cachedFrames() const {    QVector<int> frames;
    frames.reserve(composites_.size());
    for (auto it = composites_.begin(); it != composites_.end(); ++it) {
        frames.push_back(it.key());
    }
    std::sort(frames.begin(), frames.end());
    return frames;
}

QImage FrameCache::composite(int frame) {
    if (document_ == nullptr || !checkStage()) {
        return QImage();
    }
    const int clamped =
        std::max(1, std::min(frame, document_->document().FrameCount()));
    auto hit = composites_.find(clamped);
    if (hit != composites_.end()) {
        ++hits_;
        touch(hit->lastAccess);
        return hit->image;
    }
    ++misses_;
    // Compositing-only edits keep layer pixels: reassemble without vector work.
    const QImage assembled = assembleFromLayers(clamped);
    if (!assembled.isNull()) {
        storeComposite(clamped, assembled);
        emit cacheChanged();
        emit frameReady(clamped);
        return assembled;
    }
    // Cold miss: bake synchronously so scrubbing is always correct. The worker
    // prefetch makes this rare during playback, not impossible.
    const FrameCacheResult result = bakeComposite(
        document_->document(), clamped, generation_, kCacheTolerance);
    for (const FrameCacheLayerResult& layer : result.layers) {
        storeLayer(layer.layerId, clamped, layer.image);
    }
    storeComposite(clamped, result.composite);
    ++syncBakes_;
    evictToFit();
    emit cacheChanged();
    emit frameReady(clamped);
    return result.composite;
}

QImage FrameCache::layerImage(uint64_t layerId, int frame) {
    if (document_ == nullptr || !checkStage()) {
        return QImage();
    }
    const int clamped =
        std::max(1, std::min(frame, document_->document().FrameCount()));
    auto hit = layers_.find(LayerKey{layerId, clamped});
    if (hit != layers_.end()) {
        ++hits_;
        touch(hit->lastAccess);
        return hit->image;
    }
    ++misses_;
    const QImage image = bakeLayerImage(document_->document(), layerId,
                                        clamped, kCacheTolerance);
    if (!image.isNull()) {
        storeLayer(layerId, clamped, image);
        evictToFit();
        emit cacheChanged();
    }
    return image;
}

void FrameCache::prefetchAround(int playhead) {
    setPlayhead(playhead);
    if (document_ == nullptr || !checkStage()) {
        return;
    }
    if (pendingJobs_ >= kMaxPendingJobs) {
        return;
    }
    const icg::anim::AnimDocument& model = document_->document();
    const int count = model.FrameCount();
    std::vector<int> missing;
    for (int frame = std::max(1, playhead_ - kPrefetchBack);
         frame <= std::min(count, playhead_ + kPrefetchForward); ++frame) {
        if (!composites_.contains(frame)) {
            missing.push_back(frame);
        }
    }
    if (missing.empty()) {
        return;
    }
    // One shared snapshot for the whole burst: copying the document once, not
    // once per frame. The worker owns it via shared_ptr, so later edits cannot
    // touch what the bake reads (and the generation drops it if they land).
    auto snapshot =
        std::make_shared<icg::anim::AnimDocument>(model);
    for (int frame : missing) {
        if (pendingJobs_ >= kMaxPendingJobs) {
            break;
        }
        FrameCacheJob job;
        job.generation = generation_;
        job.frame = frame;
        job.snapshot = snapshot;
        // Queued: runs on the worker thread, never inline here.
        QMetaObject::invokeMethod(worker_, "bake", Qt::QueuedConnection,
                                  Q_ARG(FrameCacheJob, job));
        ++pendingJobs_;
    }
}

void FrameCache::invalidate(uint64_t layerId, int firstFrame, int lastFrame,
                            bool compositeOnly) {
    if (firstFrame > lastFrame) {
        return; // display-only edit: no pixels anywhere
    }
    ++generation_; // in-flight worker results are now stale - drop on arrival
    // ...and stop throttling on them: the pending count restarts so a
    // prefetch right after an edit isn't capped by jobs that can no longer
    // contribute. (Late arrivals still decrement, floored at zero.)
    pendingJobs_ = 0;
    if (document_ != nullptr) {
        checkStage();
    }
    bool dropped = false;
    if (compositeOnly) {
        // Layer pixels intact: only assembled frames drop. The next composite
        // reassembles from the layer tier with zero vector work.
        for (auto it = composites_.begin(); it != composites_.end();) {
            const int frame = it.key();
            if (frame >= firstFrame && frame <= lastFrame) {
                bytesUsed_ -= static_cast<qint64>(it->image.sizeInBytes());
                it = composites_.erase(it);
                dropped = true;
            } else {
                ++it;
            }
        }
    } else if (layerId == 0) {
        for (auto it = layers_.begin(); it != layers_.end();) {
            if (it.key().frame >= firstFrame && it.key().frame <= lastFrame) {
                bytesUsed_ -= static_cast<qint64>(it->image.sizeInBytes());
                it = layers_.erase(it);
                dropped = true;
            } else {
                ++it;
            }
        }
        for (auto it = composites_.begin(); it != composites_.end();) {
            if (it.key() >= firstFrame && it.key() <= lastFrame) {
                bytesUsed_ -= static_cast<qint64>(it->image.sizeInBytes());
                it = composites_.erase(it);
                dropped = true;
            } else {
                ++it;
            }
        }
    } else {
        // One layer's content changed: its images in range go, plus every
        // assembled frame in range (they contain that layer's pixels).
        for (auto it = layers_.begin(); it != layers_.end();) {
            if (it.key().layerId == layerId && it.key().frame >= firstFrame &&
                it.key().frame <= lastFrame) {
                bytesUsed_ -= static_cast<qint64>(it->image.sizeInBytes());
                it = layers_.erase(it);
                dropped = true;
            } else {
                ++it;
            }
        }
        // Map sweep, not a frame loop: the range is routinely [N, INT_MAX].
        for (auto it = composites_.begin(); it != composites_.end();) {
            if (it.key() >= firstFrame && it.key() <= lastFrame) {
                bytesUsed_ -= static_cast<qint64>(it->image.sizeInBytes());
                it = composites_.erase(it);
                dropped = true;
            } else {
                ++it;
            }
        }
    }
    bytesUsed_ = std::max<qint64>(0, bytesUsed_);
    if (dropped) {
        emit cacheChanged();
    }
}

void FrameCache::clear() {
    ++generation_;
    composites_.clear();
    layers_.clear();
    bytesUsed_ = 0;
    stageWidth_ = 0;
    stageHeight_ = 0;
    pendingJobs_ = 0; // as in invalidate(): in-flight results are stale now
    emit cacheChanged();
}

void FrameCache::onWorkerBaked(const FrameCacheResult& result) {
    if (pendingJobs_ > 0) {
        --pendingJobs_;
    }
    if (result.generation != generation_) {
        return; // an edit landed mid-bake: stale pixels, never store
    }
    if (document_ == nullptr) {
        return;
    }
    const icg::anim::AnimDocument& model = document_->document();
    if (result.stageWidth != model.stageWidth ||
        result.stageHeight != model.stageHeight) {
        return; // stage moved mid-bake: entry would be mis-sized
    }
    checkStage();
    for (const FrameCacheLayerResult& layer : result.layers) {
        if (!layers_.contains(LayerKey{layer.layerId, result.frame})) {
            storeLayer(layer.layerId, result.frame, layer.image);
        }
    }
    if (!composites_.contains(result.frame)) {
        storeComposite(result.frame, result.composite);
    }
    evictToFit();
    emit frameReady(result.frame);
    emit cacheChanged();
}

bool FrameCache::checkStage() {
    if (document_ == nullptr) {
        return false;
    }
    const icg::anim::AnimDocument& model = document_->document();
    if (stageWidth_ == 0 && stageHeight_ == 0) {
        stageWidth_ = model.stageWidth;
        stageHeight_ = model.stageHeight;
        return true;
    }
    if (stageWidth_ != model.stageWidth || stageHeight_ != model.stageHeight) {
        // Stage resize: every entry is mis-sized. The SetStageSize command
        // already invalidates all frames, so this is only belt-and-braces for
        // paths that bypass commands (there are none today).
        composites_.clear();
        layers_.clear();
        bytesUsed_ = 0;
        stageWidth_ = model.stageWidth;
        stageHeight_ = model.stageHeight;
        emit cacheChanged();
    }
    return true;
}

void FrameCache::storeComposite(int frame, const QImage& image) {
    if (image.isNull()) {
        return;
    }
    auto found = composites_.find(frame);
    if (found != composites_.end()) {
        bytesUsed_ -= static_cast<qint64>(found->image.sizeInBytes());
    }
    Entry entry;
    entry.image = image;
    touch(entry.lastAccess);
    composites_.insert(frame, entry);
    bytesUsed_ += static_cast<qint64>(image.sizeInBytes());
}

void FrameCache::storeLayer(uint64_t layerId, int frame, const QImage& image) {
    if (image.isNull()) {
        return;
    }
    const LayerKey key{layerId, frame};
    auto found = layers_.find(key);
    if (found != layers_.end()) {
        bytesUsed_ -= static_cast<qint64>(found->image.sizeInBytes());
    }
    Entry entry;
    entry.image = image;
    touch(entry.lastAccess);
    layers_.insert(key, entry);
    bytesUsed_ += static_cast<qint64>(image.sizeInBytes());
}

bool FrameCache::isProtected(int frame) const {
    return frame >= playhead_ - kPrefetchBack &&
           frame <= playhead_ + kPrefetchForward;
}

void FrameCache::evictToFit() {
    while (bytesUsed_ > budgetBytes_ &&
           (!composites_.isEmpty() || !layers_.isEmpty())) {
        // Oldest last-access first; protected frames (near the playhead) only
        // when nothing else can go, so playback never evicts what it shows
        // next. Two sweeps keep the rule obvious: unprotected oldest, else
        // oldest overall.
        bool victimIsComposite = false;
        int victimComposite = 0;
        LayerKey victimLayer;
        quint64 oldest = 0;
        bool found = false;
        for (int pass = 0; pass < 2 && !found; ++pass) {
            const bool wantUnprotected = (pass == 0);
            oldest = 0;
            for (auto it = composites_.begin(); it != composites_.end();
                 ++it) {
                if (wantUnprotected && isProtected(it.key())) {
                    continue;
                }
                if (oldest == 0 || it->lastAccess < oldest) {
                    oldest = it->lastAccess;
                    victimIsComposite = true;
                    victimComposite = it.key();
                    found = true;
                }
            }
            for (auto it = layers_.begin(); it != layers_.end(); ++it) {
                const bool unprotected = !isProtected(it.key().frame);
                if (wantUnprotected && !unprotected) {
                    continue;
                }
                if (oldest == 0 || it->lastAccess < oldest) {
                    oldest = it->lastAccess;
                    victimIsComposite = false;
                    victimLayer = it.key();
                    found = true;
                }
            }
        }
        if (!found) {
            break; // cannot happen (non-empty maps), but never spin
        }
        if (victimIsComposite) {
            auto it = composites_.find(victimComposite);
            if (it != composites_.end()) {
                bytesUsed_ -= static_cast<qint64>(it->image.sizeInBytes());
                composites_.erase(it);
            }
        } else {
            auto it = layers_.find(victimLayer);
            if (it != layers_.end()) {
                bytesUsed_ -= static_cast<qint64>(it->image.sizeInBytes());
                layers_.erase(it);
            }
        }
        bytesUsed_ = std::max<qint64>(0, bytesUsed_);
    }
}

QImage FrameCache::assembleFromLayers(int frame) {
    if (document_ == nullptr) {
        return QImage();
    }
    const icg::anim::AnimDocument& model = document_->document();
    QImage assembled(model.stageWidth, model.stageHeight,
                     QImage::Format_ARGB32_Premultiplied);
    assembled.fill(Qt::transparent);
    QPainter painter(&assembled);
    // Layers are stored top-first; compositing runs bottom-first.
    for (size_t i = model.layers.size(); i-- > 0;) {
        const icg::anim::AnimLayer& layer = model.layers[i];
        if (!layer.visible) {
            continue;
        }
        if (layer.AtOrBefore(frame) == nullptr) {
            continue; // contributes nothing on this frame
        }
        auto found = layers_.find(LayerKey{layer.id, frame});
        if (found == layers_.end()) {
            return QImage(); // a contributor is missing: full bake instead
        }
        touch(found->lastAccess);
        painter.drawImage(0, 0, found->image);
    }
    painter.end();
    return assembled;
}

QImage FrameCache::bakeLayerImage(const icg::anim::AnimDocument& snapshot,
                                  uint64_t layerId, int frame,
                                  float tolerance) {
    const icg::anim::AnimLayer* layer = snapshot.FindLayerById(layerId);
    if (layer == nullptr || !layer->visible) {
        return QImage();
    }
    const icg::anim::AnimKeyframe* key = layer->AtOrBefore(frame);
    if (key == nullptr || key->shapes.empty()) {
        return QImage();
    }
    QImage image(snapshot.stageWidth, snapshot.stageHeight,
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    bool any = false;
    for (const icg::anim::AnimShape& shape : key->shapes) {
        const icg::anim::ResolvedStyle style =
            icg::anim::ResolveShapeStyle(shape, *key);
        const icg::anim::FlatPath flat =
            icg::anim::Flatten(shape.path, tolerance);
        if (flat.polylines.empty()) {
            continue;
        }
        icg::anim::ResolvedShape resolved;
        resolved.shapeId = shape.id;
        resolved.path = flat;
        resolved.matrix = style.matrix;
        resolved.hasFill = style.hasFill;
        resolved.fill = style.fill;
        resolved.hasStroke = style.hasStroke;
        resolved.stroke = style.stroke;
        resolved.strokeWidth = style.strokeWidth;
        resolved.cap = style.cap;
        resolved.join = style.join;
        resolved.layerOutline = layer->outline;
        resolved.drawable = true;
        paintResolvedShape(painter, resolved, tolerance);
        any = true;
    }
    painter.end();
    if (!any) {
        return QImage();
    }
    return image;
}

FrameCacheResult FrameCache::bakeComposite(
    const icg::anim::AnimDocument& snapshot, int frame, quint64 generation,
    float tolerance) {
    FrameCacheResult result;
    result.generation = generation;
    result.frame = frame;
    result.stageWidth = snapshot.stageWidth;
    result.stageHeight = snapshot.stageHeight;
    QImage assembled(snapshot.stageWidth, snapshot.stageHeight,
                     QImage::Format_ARGB32_Premultiplied);
    assembled.fill(Qt::transparent);
    QPainter painter(&assembled);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (size_t i = snapshot.layers.size(); i-- > 0;) {
        const icg::anim::AnimLayer& layer = snapshot.layers[i];
        if (!layer.visible) {
            continue;
        }
        const QImage layerImage =
            bakeLayerImage(snapshot, layer.id, frame, tolerance);
        if (layerImage.isNull()) {
            continue;
        }
        painter.drawImage(0, 0, layerImage);
        FrameCacheLayerResult stored;
        stored.layerId = layer.id;
        stored.image = layerImage;
        result.layers.push_back(std::move(stored));
    }
    painter.end();
    result.composite = assembled;
    return result;
}
