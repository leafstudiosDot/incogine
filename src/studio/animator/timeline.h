// Incogine Animator - M4 timeline widget (layers panel + frame grid).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Flash-style layout in one dock: a transport bar (stop/play/pause, loop,
// frame readout, onion toggle, layer add/remove), then a single custom-painted
// grid - layer rows on the left (color chip, name, eye/lock/outline), frames
// as columns on the right (key = filled diamond, blank = hollow, spans
// shaded, tween spans hatched as reserved-until-M7, playhead red line).
//
// The widget owns NO model: layers/frames come from AnimatorDocument, edits go
// through its command methods (undoable), painting targets AnimatorCanvas.
// The copy clipboard lives here (read-only model copies); paste pushes a
// PasteFramesCommand. Playback is a QTimer at document FPS.
//
// ASCII-only by repo convention (src/studio is ASCII): transport icons come
// from QStyle standard pixmaps, never from glyphs.
#pragma once

#include <QPushButton>
#include <QWidget>

#include <cstdint>
#include <vector>

#include "animation/anim_document.h"

class AnimatorCanvas;
class AnimatorDocument;
class FrameCache;
class QCheckBox;
class QLabel;
class QScrollBar;
class QTimer;

// Custom-painted layers + frame grid. Owned by TimelineWidget, which provides
// the model accessors below so the grid never touches document/canvas types
// it does not need.
class TimelineGrid : public QWidget {
    Q_OBJECT

public:
    explicit TimelineGrid(class TimelineWidget* owner, QWidget* parent = nullptr);

    // Layout metrics, shared with hit-testing. Fixed cell size keeps the grid
    // math exact (no fractional accumulation across hundreds of frames).
    static constexpr int kHeaderWidth = 170;
    static constexpr int kCellWidth = 13;
    static constexpr int kRowHeight = 22;
    static constexpr int kRulerHeight = 20;
    static constexpr int kCacheStripHeight = 6;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int frameAtX(int x) const;
    int rowAtY(int y) const;
    int xForFrame(int frame) const;
    int yForRow(int row) const;
    QRect eyeRect(int row) const;
    QRect lockRect(int row) const;
    QRect outlineRect(int row) const;
    QRect colorRect(int row) const;

    TimelineWidget* owner_;
    bool scrubbing_ = false;
    bool workDragging_ = false;
};

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(AnimatorDocument* document, AnimatorCanvas* canvas,
                            QWidget* parent = nullptr);

    // --- state the grid reads ---
    AnimatorDocument* document() const { return document_; }
    AnimatorCanvas* canvas() const { return canvas_; }
    int playhead() const { return playhead_; }
    int selectedRow() const { return selectedRow_; }
    int firstFrame() const { return firstFrame_; }
    int firstRow() const { return firstRow_; }
    bool hasWorkArea() const { return workStart_ >= 1; }
    int workStart() const { return workStart_; }
    int workEnd() const { return workEnd_; }
    bool isPlaying() const { return playing_; }

    // --- test accessors ---
    int rowCount() const;
    uint64_t currentLayerId() const;
    // Current tick interval in ms (from document FPS; retimed mid-playback).
    int playbackIntervalMs() const;

    // RAM frame cache (1.3, non-owning): feeds the cache strip above the
    // ruler. Null until the window wires the shared cache.
    void setFrameCache(FrameCache* cache);
    FrameCache* frameCache() const { return frameCache_; }

signals:
    // Emitted when playback starts or stops (pause, stop, end-of-timeline).
    // The window relays it to the canvas, which switches between the cached
    // playback blit and the exact scene path.
    void playingChanged(bool playing);

public slots:
    void setPlayhead(int frame);
    void refreshAll();

private slots:
    void onPlayPause();
    void onStop();
    void onLoopToggled(bool loop);
    void onOnionToggled(bool enabled);
    void onAddLayer();
    void onDeleteLayer();
    void onPlaybackTick();
    void onHScroll(int value);
    void onVScroll(int value);

private:
    friend class TimelineGrid;
    // Grid callbacks (row/frame already validated by the grid).
    void selectRow(int row);
    void setFirstFrame(int frame);
    void setFirstRow(int row);
    void beginScrub(int frame);
    void scrubTo(int frame);
    void endScrub();
    void beginWorkArea(int frame);
    void extendWorkArea(int frame);
    void clearWorkArea();
    void insertBlankAt(int row, int frame);
    void copyFrames(int row, int frame);
    void pasteFrames(int row, int frame);
    uint64_t layerIdAtRow(int row) const;
    int rowForLayer(uint64_t layerId) const;
    void clampPlayhead();
    void updateTransport();

    AnimatorDocument* document_;
    AnimatorCanvas* canvas_;
    FrameCache* frameCache_ = nullptr;
    TimelineGrid* grid_;
    QScrollBar* hScroll_;
    QScrollBar* vScroll_;
    QPushButton* playButton_;
    QPushButton* deleteLayerButton_;
    QPushButton* onionButton_;
    QCheckBox* loopBox_;
    QLabel* frameLabel_;
    QLabel* fpsLabel_;
    QTimer* playTimer_;
    int playhead_ = 1;
    int selectedRow_ = 0;
    int firstFrame_ = 1;
    int firstRow_ = 0;
    int workStart_ = -1;
    int workEnd_ = -1;
    bool playing_ = false;
    std::vector<icg::anim::AnimKeyframe> clipboard_;
};
