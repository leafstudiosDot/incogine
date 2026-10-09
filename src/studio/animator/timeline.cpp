// Incogine Animator - M4 timeline widget (layers panel + frame grid).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Layout: transport bar (stop/play, loop, frame readout, fps, onion toggle,
// add/remove layer), then the grid with a horizontal frame scrollbar and a
// vertical row scrollbar. The cache strip above the ruler shows the 1.3 RAM
// cache: a filled bar per baked composite frame (see setFrameCache).
//
// ASCII-only by repo convention: no glyph icons; transport uses QStyle
// standard pixmaps and text buttons.

#include "timeline.h"

#include <QApplication>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "canvas.h"
#include "document.h"
#include "framecache.h"

using icg::anim::AnimKeyframe;
using icg::anim::AnimLayer;
using icg::anim::KeyframeKind;
using icg::anim::TweenType;
using icg::anim::Vec2;

// ------------------------------------------------------------ grid --

TimelineGrid::TimelineGrid(TimelineWidget* owner, QWidget* parent)
    : QWidget(parent), owner_(owner) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumHeight(kRulerHeight + kCacheStripHeight + kRowHeight * 2);
}

int TimelineGrid::frameAtX(int x) const {
    if (x < kHeaderWidth) {
        return -1;
    }
    return owner_->firstFrame() + (x - kHeaderWidth) / kCellWidth;
}

int TimelineGrid::rowAtY(int y) const {
    const int gridTop = kRulerHeight + kCacheStripHeight;
    if (y < gridTop) {
        return -1;
    }
    return owner_->firstRow() + (y - gridTop) / kRowHeight;
}

int TimelineGrid::xForFrame(int frame) const {
    return kHeaderWidth + (frame - owner_->firstFrame()) * kCellWidth;
}

int TimelineGrid::yForRow(int row) const {
    const int gridTop = kRulerHeight + kCacheStripHeight;
    return gridTop + (row - owner_->firstRow()) * kRowHeight;
}

QRect TimelineGrid::eyeRect(int row) const {
    return QRect(4, yForRow(row) + 3, 16, 16);
}

QRect TimelineGrid::lockRect(int row) const {
    return QRect(22, yForRow(row) + 3, 16, 16);
}

QRect TimelineGrid::outlineRect(int row) const {
    return QRect(40, yForRow(row) + 3, 16, 16);
}

QRect TimelineGrid::colorRect(int row) const {
    return QRect(58, yForRow(row) + 3, 14, 16);
}

void TimelineGrid::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    AnimatorDocument* document = owner_->document();
    if (document == nullptr) {
        return;
    }
    const icg::anim::AnimDocument& model = document->document();
    const int playhead = owner_->playhead();
    const int selectedRow = owner_->selectedRow();
    const QPalette& palette = this->palette();
    const QColor base = palette.color(QPalette::Base);
    const QColor header = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    const QColor mid = palette.color(QPalette::Mid);
    const QColor highlight = palette.color(QPalette::Highlight);

    painter.fillRect(rect(), base);

    const int rows = static_cast<int>(model.layers.size());
    const int gridTop = kRulerHeight + kCacheStripHeight;
    const int lastFrame =
        owner_->firstFrame() + (width() - kHeaderWidth) / kCellWidth + 1;

    // Frame ruler: numbers every 5, ticks every frame, work area shade.
    painter.fillRect(0, 0, width(), kRulerHeight, header);
    if (owner_->hasWorkArea()) {
        const int x0 = xForFrame(owner_->workStart());
        const int x1 = xForFrame(owner_->workEnd() + 1);
        painter.fillRect(x0, 0, x1 - x0, kRulerHeight,
                         highlight.lighter(160));
    }
    painter.setPen(mid);
    for (int frame = owner_->firstFrame(); frame <= lastFrame + 1; ++frame) {
        const int x = xForFrame(frame);
        if (frame % 5 == 0 || frame == 1) {
            painter.drawLine(x, kRulerHeight - 8, x, kRulerHeight);
            painter.setPen(text);
            painter.drawText(x + 2, 0, kCellWidth * 4, kRulerHeight - 2,
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QString::number(frame));
            painter.setPen(mid);
        } else {
            painter.drawLine(x, kRulerHeight - 4, x, kRulerHeight);
        }
    }
    // Cache strip (1.3 draws cached-frame bars here; reserved, painted empty).
    painter.fillRect(0, kRulerHeight, width(), kCacheStripHeight,
                     base.darker(105));
    // Cached frames read green-ish: a filled bar means the composite is baked
    // and playback/scrub serves it without vector work. No cache wired (or
    // nothing baked yet) leaves the strip empty - never an error state.
    if (owner_->frameCache() != nullptr) {
        const QColor cached = highlight.darker(115);
        for (int frame = owner_->firstFrame(); frame <= lastFrame; ++frame) {
            if (frame > model.FrameCount()) {
                break;
            }
            if (owner_->frameCache()->isCached(frame)) {
                const int x = xForFrame(frame);
                painter.fillRect(x + 1, kRulerHeight + 1, kCellWidth - 2,
                                 kCacheStripHeight - 2, cached);
            }
        }
    }
    painter.setPen(mid);
    painter.drawLine(0, gridTop - 1, width(), gridTop - 1);

    // Rows: header (chip, name, eye/lock/outline) + frame cells.
    for (int row = owner_->firstRow();
         row < rows && yForRow(row) < height(); ++row) {
        const AnimLayer& layer = model.layers[static_cast<size_t>(row)];
        const int y = yForRow(row);
        const bool selected = (row == selectedRow);
        if (selected) {
            painter.fillRect(0, y, width(), kRowHeight, highlight.lighter(170));
        }
        // Header.
        painter.fillRect(0, y, kHeaderWidth, kRowHeight,
                         selected ? highlight.lighter(170) : header);
        const QColor chip(layer.color.r, layer.color.g, layer.color.b);
        painter.fillRect(colorRect(row), chip);
        painter.setPen(mid);
        painter.drawRect(colorRect(row));
        painter.setPen(layer.locked ? mid : text);
        painter.drawText(76, y, kHeaderWidth - 76 - 58, kRowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromStdString(layer.name));
        painter.setPen(text);
        painter.drawText(eyeRect(row), Qt::AlignCenter,
                         layer.visible ? QStringLiteral("E") : QStringLiteral("-"));
        painter.drawText(lockRect(row), Qt::AlignCenter,
                         layer.locked ? QStringLiteral("L") : QStringLiteral("-"));
        painter.drawText(outlineRect(row), Qt::AlignCenter,
                         layer.outline ? QStringLiteral("O") : QStringLiteral("-"));
        painter.setPen(mid);
        painter.drawRect(eyeRect(row));
        painter.drawRect(lockRect(row));
        painter.drawRect(outlineRect(row));
        painter.drawLine(kHeaderWidth, y, kHeaderWidth, y + kRowHeight);

        // Cells: span shading, tween hatch, keyframe diamonds.
        for (int frame = owner_->firstFrame(); frame <= lastFrame; ++frame) {
            const int x = xForFrame(frame);
            const AnimKeyframe* at = layer.Find(frame);
            const AnimKeyframe* holding = layer.AtOrBefore(frame);
            if (holding != nullptr) {
                painter.fillRect(x + 1, y + 1, kCellWidth - 1, kRowHeight - 1,
                                 base.darker(108));
                // Reserved tween spans hatch until M7 samples them.
                if (holding->tweenIn.type != TweenType::None) {
                    painter.setPen(mid);
                    for (int dx = 0; dx < kCellWidth; dx += 4) {
                        painter.drawLine(x + 1 + dx, y + kRowHeight - 2,
                                         x + 3 + dx, y + 2);
                    }
                }
            }
            if (at != nullptr) {
                const bool blank = (at->kind == KeyframeKind::Blank);
                const QPointF center(x + kCellWidth / 2.0,
                                     y + kRowHeight / 2.0);
                const double r = 4.0;
                QPainterPath diamond;
                diamond.moveTo(center.x(), center.y() - r);
                diamond.lineTo(center.x() + r, center.y());
                diamond.lineTo(center.x(), center.y() + r);
                diamond.lineTo(center.x() - r, center.y());
                diamond.closeSubpath();
                painter.setPen(QPen(selected ? highlight.darker(140)
                                             : Qt::black));
                painter.setBrush(blank ? Qt::NoBrush
                                       : (selected ? highlight.darker(140)
                                                   : Qt::black));
                painter.drawPath(diamond);
            }
        }
        painter.setPen(mid);
        painter.drawLine(0, y + kRowHeight, width(), y + kRowHeight);
    }

    // Playhead: red line plus a handle in the ruler.
    {
        const int x = xForFrame(playhead) + kCellWidth / 2;
        painter.setPen(QPen(QColor(220, 40, 40)));
        painter.drawLine(x, 0, x, height());
        painter.setBrush(QColor(220, 40, 40));
        painter.setPen(Qt::NoPen);
        QPainterPath handle;
        handle.moveTo(x - 5, 0);
        handle.lineTo(x + 5, 0);
        handle.lineTo(x, 8);
        handle.closeSubpath();
        painter.drawPath(handle);
    }
}

void TimelineGrid::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const QPoint pos = event->pos();
    if (pos.y() < kRulerHeight + kCacheStripHeight) {
        const int frame = std::max(1, frameAtX(pos.x()));
        if (event->modifiers() & Qt::ShiftModifier) {
            owner_->beginWorkArea(frame);
        } else {
            owner_->clearWorkArea();
            owner_->beginScrub(frame);
        }
        scrubbing_ = (event->modifiers() & Qt::ShiftModifier) == 0;
        workDragging_ = !scrubbing_;
        return;
    }
    const int row = rowAtY(pos.y());
    if (pos.x() < kHeaderWidth) {
        if (row < 0) {
            return;
        }
        owner_->selectRow(row);
        if (eyeRect(row).contains(pos)) {
            AnimatorDocument* document = owner_->document();
            const AnimLayer* layer =
                document->document().FindLayerById(owner_->currentLayerId());
            if (layer != nullptr) {
                document->setLayerVisible(layer->id, !layer->visible);
            }
        } else if (lockRect(row).contains(pos)) {
            AnimatorDocument* document = owner_->document();
            const AnimLayer* layer =
                document->document().FindLayerById(owner_->currentLayerId());
            if (layer != nullptr) {
                document->setLayerLocked(layer->id, !layer->locked);
            }
        } else if (outlineRect(row).contains(pos)) {
            AnimatorDocument* document = owner_->document();
            const AnimLayer* layer =
                document->document().FindLayerById(owner_->currentLayerId());
            if (layer != nullptr) {
                document->setLayerOutline(layer->id, !layer->outline);
            }
        } else if (colorRect(row).contains(pos)) {
            AnimatorDocument* document = owner_->document();
            const AnimLayer* layer =
                document->document().FindLayerById(owner_->currentLayerId());
            if (layer != nullptr) {
                const QColor picked = QColorDialog::getColor(
                    QColor(layer->color.r, layer->color.g, layer->color.b),
                    this, tr("Layer color"));
                if (picked.isValid()) {
                    document->setLayerColor(
                        layer->id, icg::anim::AnimColor(picked.red(),
                                                       picked.green(),
                                                       picked.blue(), 255));
                }
            }
        }
        return;
    }
    const int frame = frameAtX(pos.x());
    if (frame >= 1) {
        owner_->beginScrub(frame);
        scrubbing_ = true;
        workDragging_ = false;
    }
}

void TimelineGrid::mouseMoveEvent(QMouseEvent* event) {
    if (!scrubbing_ && !workDragging_) {
        return;
    }
    const int frame = std::max(1, frameAtX(event->pos().x()));
    if (scrubbing_) {
        owner_->scrubTo(frame);
    } else {
        owner_->extendWorkArea(frame);
    }
}

void TimelineGrid::mouseReleaseEvent(QMouseEvent* event) {
    (void)event;
    if (scrubbing_) {
        owner_->endScrub();
    }
    scrubbing_ = false;
    workDragging_ = false;
}

void TimelineGrid::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const QPoint pos = event->pos();
    if (pos.x() < kHeaderWidth ||
        pos.y() < kRulerHeight + kCacheStripHeight) {
        return;
    }
    const int row = rowAtY(pos.y());
    const int frame = frameAtX(pos.x());
    if (row >= 0 && frame >= 1) {
        owner_->selectRow(row);
        owner_->insertBlankAt(row, frame);
    }
}

void TimelineGrid::keyPressEvent(QKeyEvent* event) {
    AnimatorDocument* document = owner_->document();
    if (document == nullptr) {
        QWidget::keyPressEvent(event);
        return;
    }
    const uint64_t layerId = owner_->currentLayerId();
    const int frame = owner_->playhead();
    const bool ctrl =
        (event->modifiers() & Qt::ControlModifier) != 0;
    if (event->key() == Qt::Key_F5 && !(event->modifiers() & Qt::ShiftModifier)) {
        document->insertFrames(frame, 1);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F5 && (event->modifiers() & Qt::ShiftModifier)) {
        document->removeFrames(frame, 1);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F6) {
        if (layerId != 0) {
            document->insertKeyframe(layerId, frame, false);
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F7) {
        if (layerId != 0) {
            document->insertKeyframe(layerId, frame, true);
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (layerId != 0) {
            document->clearKeyframe(layerId, frame);
        }
        event->accept();
        return;
    }
    if (ctrl && event->key() == Qt::Key_C) {
        owner_->copyFrames(owner_->selectedRow(), frame);
        event->accept();
        return;
    }
    if (ctrl && event->key() == Qt::Key_V) {
        owner_->pasteFrames(owner_->selectedRow(), frame);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        owner_->onPlayPause();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void TimelineGrid::contextMenuEvent(QContextMenuEvent* event) {
    const QPoint pos = event->pos();
    QMenu menu(this);
    if (pos.x() < kHeaderWidth && pos.y() >= kRulerHeight + kCacheStripHeight) {
        const int row = rowAtY(pos.y());
        if (row < 0) {
            return;
        }
        owner_->selectRow(row);
        QAction* add = menu.addAction(tr("Insert Layer"));
        QAction* del = menu.addAction(tr("Delete Layer"));
        QAction* rename = menu.addAction(tr("Rename Layer..."));
        QAction* up = menu.addAction(tr("Move Layer Up"));
        QAction* down = menu.addAction(tr("Move Layer Down"));
        QAction* picked = menu.exec(event->globalPos());
        if (picked == add) {
            owner_->onAddLayer();
        } else if (picked == del) {
            owner_->onDeleteLayer();
        } else if (picked == rename) {
            AnimatorDocument* document = owner_->document();
            const AnimLayer* layer =
                document->document().FindLayerById(owner_->currentLayerId());
            if (layer != nullptr) {
                const QString name = QInputDialog::getText(
                    this, tr("Rename Layer"), tr("Name:"),
                    QLineEdit::Normal,
                    QString::fromStdString(layer->name));
                if (!name.isEmpty()) {
                    document->renameLayer(layer->id, name);
                }
            }
        } else if (picked == up || picked == down) {
            AnimatorDocument* document = owner_->document();
            const size_t from = static_cast<size_t>(row);
            const size_t to =
                picked == up ? (from == 0 ? from : from - 1) : from + 1;
            if (to < document->document().layers.size()) {
                document->moveLayer(from, to);
                owner_->selectRow(static_cast<int>(to));
            }
        }
        return;
    }
    const int frame = frameAtX(pos.x());
    if (frame < 1) {
        return;
    }
    const int row = std::max(0, rowAtY(pos.y()));
    owner_->selectRow(row);
    QAction* insertF = menu.addAction(tr("Insert Frame\tF5"));
    QAction* removeF = menu.addAction(tr("Remove Frame\tShift+F5"));
    menu.addSeparator();
    QAction* insertK = menu.addAction(tr("Insert Keyframe\tF6"));
    QAction* insertB = menu.addAction(tr("Insert Blank Keyframe\tF7"));
    QAction* clearK = menu.addAction(tr("Clear Keyframe\tDel"));
    menu.addSeparator();
    QAction* copy = menu.addAction(tr("Copy Keyframe\tCtrl+C"));
    QAction* paste = menu.addAction(tr("Paste Frames\tCtrl+V"));
    menu.addSeparator();
    QAction* clearWork = menu.addAction(tr("Clear Work Area"));
    QAction* picked = menu.exec(event->globalPos());
    AnimatorDocument* document = owner_->document();
    const uint64_t layerId = owner_->currentLayerId();
    if (picked == insertF) {
        document->insertFrames(frame, 1);
        owner_->setPlayhead(frame);
    } else if (picked == removeF) {
        document->removeFrames(frame, 1);
    } else if (picked == insertK) {
        document->insertKeyframe(layerId, frame, false);
    } else if (picked == insertB) {
        document->insertKeyframe(layerId, frame, true);
    } else if (picked == clearK) {
        document->clearKeyframe(layerId, frame);
    } else if (picked == copy) {
        owner_->copyFrames(row, frame);
    } else if (picked == paste) {
        owner_->pasteFrames(row, frame);
    } else if (picked == clearWork) {
        owner_->clearWorkArea();
    }
}

// ---------------------------------------------------------- widget --

TimelineWidget::TimelineWidget(AnimatorDocument* document,
                               AnimatorCanvas* canvas, QWidget* parent)
    : QWidget(parent), document_(document), canvas_(canvas) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    auto* transport = new QHBoxLayout();
    transport->setSpacing(4);
    auto* stopButton = new QPushButton(
        style()->standardIcon(QStyle::SP_MediaStop), QString(), this);
    stopButton->setToolTip(tr("Stop"));
    connect(stopButton, &QPushButton::clicked, this, &TimelineWidget::onStop);
    transport->addWidget(stopButton);

    playButton_ = new QPushButton(
        style()->standardIcon(QStyle::SP_MediaPlay), QString(), this);
    playButton_->setToolTip(tr("Play / pause (Space when the timeline is focused)"));
    playButton_->setCheckable(true);
    connect(playButton_, &QPushButton::clicked, this,
            &TimelineWidget::onPlayPause);
    transport->addWidget(playButton_);

    loopBox_ = new QCheckBox(tr("Loop"), this);
    loopBox_->setChecked(document_->document().loop);
    connect(loopBox_, &QCheckBox::toggled, this, &TimelineWidget::onLoopToggled);
    transport->addWidget(loopBox_);

    frameLabel_ = new QLabel(this);
    transport->addWidget(frameLabel_);
    fpsLabel_ = new QLabel(this);
    transport->addWidget(fpsLabel_);

    onionButton_ = new QPushButton(tr("Onion"), this);
    onionButton_->setToolTip(tr("Onion skin: previous/next keyframes ghosted"));
    onionButton_->setCheckable(true);
    connect(onionButton_, &QPushButton::toggled, this,
            &TimelineWidget::onOnionToggled);
    transport->addWidget(onionButton_);

    auto* addLayer = new QPushButton(tr("+ Layer"), this);
    connect(addLayer, &QPushButton::clicked, this, &TimelineWidget::onAddLayer);
    transport->addWidget(addLayer);
    deleteLayerButton_ = new QPushButton(tr("- Layer"), this);
    deleteLayerButton_->setToolTip(tr("Delete the selected layer"));
    connect(deleteLayerButton_, &QPushButton::clicked, this,
            &TimelineWidget::onDeleteLayer);
    transport->addWidget(deleteLayerButton_);
    transport->addStretch(1);
    layout->addLayout(transport);

    auto* body = new QHBoxLayout();
    body->setSpacing(0);
    grid_ = new TimelineGrid(this, this);
    body->addWidget(grid_, 1);
    vScroll_ = new QScrollBar(Qt::Vertical, this);
    connect(vScroll_, &QScrollBar::valueChanged, this,
            &TimelineWidget::onVScroll);
    body->addWidget(vScroll_);
    layout->addLayout(body, 1);

    hScroll_ = new QScrollBar(Qt::Horizontal, this);
    connect(hScroll_, &QScrollBar::valueChanged, this,
            &TimelineWidget::onHScroll);
    layout->addWidget(hScroll_);

    playTimer_ = new QTimer(this);
    connect(playTimer_, &QTimer::timeout, this, &TimelineWidget::onPlaybackTick);
    connect(document_, &AnimatorDocument::documentChanged, this,
            &TimelineWidget::refreshAll);

    setFocusPolicy(Qt::StrongFocus);
    refreshAll();
}

void TimelineWidget::setFrameCache(FrameCache* cache) {
    if (frameCache_ != nullptr) {
        disconnect(frameCache_, nullptr, this, nullptr);
    }
    frameCache_ = cache;
    if (frameCache_ != nullptr) {
        // Bakes landing (sync or worker) move the strip bars.
        connect(frameCache_, &FrameCache::cacheChanged, grid_,
                qOverload<>(&QWidget::update));
    }
    grid_->update();
}

int TimelineWidget::rowCount() const {
    if (document_ == nullptr) {
        return 0;
    }
    return static_cast<int>(document_->document().layers.size());
}

int TimelineWidget::playbackIntervalMs() const {
    return playTimer_ != nullptr ? playTimer_->interval() : 0;
}

uint64_t TimelineWidget::currentLayerId() const {
    return layerIdAtRow(selectedRow_);
}

uint64_t TimelineWidget::layerIdAtRow(int row) const {
    if (document_ == nullptr) {
        return 0;
    }
    const auto& layers = document_->document().layers;
    if (row < 0 || row >= static_cast<int>(layers.size())) {
        return 0;
    }
    return layers[static_cast<size_t>(row)].id;
}

int TimelineWidget::rowForLayer(uint64_t layerId) const {
    if (document_ == nullptr) {
        return -1;
    }
    const auto& layers = document_->document().layers;
    for (size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].id == layerId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TimelineWidget::setPlayhead(int frame) {
    if (document_ == nullptr) {
        return;
    }
    const int clamped =
        std::max(1, std::min(frame, document_->document().FrameCount()));
    if (clamped == playhead_) {
        return;
    }
    playhead_ = clamped;
    if (canvas_ != nullptr) {
        canvas_->setCurrentFrame(playhead_);
    }
    // Keep the playhead on screen while scrubbing or playing.
    const int visible = std::max(
        1, (grid_->width() - TimelineGrid::kHeaderWidth) /
               TimelineGrid::kCellWidth);
    if (playhead_ < firstFrame_) {
        firstFrame_ = playhead_;
    } else if (playhead_ >= firstFrame_ + visible) {
        firstFrame_ = playhead_ - visible + 1;
    }
    updateTransport();
    grid_->update();
    hScroll_->setValue(firstFrame_);
}

void TimelineWidget::refreshAll() {
    if (document_ == nullptr) {
        return;
    }
    const auto& model = document_->document();
    clampPlayhead();
    selectedRow_ =
        std::max(0, std::min(selectedRow_, rowCount() - 1));
    firstFrame_ = std::max(1, std::min(firstFrame_, model.FrameCount()));
    if (hasWorkArea()) {
        workStart_ = std::max(1, std::min(workStart_, model.FrameCount()));
        workEnd_ = std::max(workStart_, std::min(workEnd_, model.FrameCount()));
    }
    hScroll_->setMinimum(1);
    hScroll_->setMaximum(std::max(1, model.FrameCount()));
    hScroll_->setValue(firstFrame_);
    const int rowsVisible = std::max(
        1, (grid_->height() - TimelineGrid::kRulerHeight -
            TimelineGrid::kCacheStripHeight) /
               TimelineGrid::kRowHeight);
    vScroll_->setMinimum(0);
    vScroll_->setMaximum(std::max(0, rowCount() - rowsVisible));
    vScroll_->setValue(firstRow_);
    {
        const QSignalBlocker block(loopBox_);
        loopBox_->setChecked(model.loop);
    }
    // Mid-playback FPS edits retime the ticks: without this the timer keeps
    // the interval from when play started while the label shows the new rate.
    // Comparing avoids restarting (and re-phasing) the timer on unrelated
    // edits, which also funnel through here via documentChanged.
    if (playing_ && playTimer_ != nullptr) {
        const int interval =
            std::max(1, 1000 / std::max(1, model.fps));
        if (playTimer_->interval() != interval) {
            playTimer_->start(interval);
        }
    }
    updateTransport();
    grid_->update();
}

void TimelineWidget::clampPlayhead() {
    if (document_ == nullptr) {
        return;
    }
    const int clamped =
        std::max(1, std::min(playhead_, document_->document().FrameCount()));
    playhead_ = clamped;
    if (canvas_ != nullptr && canvas_->currentFrame() != clamped) {
        canvas_->setCurrentFrame(clamped);
    }
}

void TimelineWidget::updateTransport() {
    if (document_ == nullptr) {
        return;
    }
    frameLabel_->setText(tr("F %1 / %2").arg(playhead_).arg(
        document_->document().FrameCount()));
    fpsLabel_->setText(tr("%1 fps").arg(document_->document().fps));
    playButton_->setIcon(style()->standardIcon(
        playing_ ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    playButton_->setChecked(playing_);
}

void TimelineWidget::onPlayPause() {
    if (playing_) {
        playing_ = false;
        playTimer_->stop();
        updateTransport();
        emit playingChanged(false);
        return;
    }
    if (document_ == nullptr) {
        return;
    }
    // Restart from the work area (or frame 1) when starting at the end, so
    // pressing play always does something visible.
    const int end = hasWorkArea() ? workEnd_ : document_->document().FrameCount();
    const int start = hasWorkArea() ? workStart_ : 1;
    if (playhead_ >= end) {
        setPlayhead(start);
    }
    playing_ = true;
    playTimer_->start(std::max(1, 1000 / std::max(1, document_->document().fps)));
    updateTransport();
    emit playingChanged(true);
}

void TimelineWidget::onStop() {
    const bool wasPlaying = playing_;
    playing_ = false;
    playTimer_->stop();
    setPlayhead(hasWorkArea() ? workStart_ : 1);
    updateTransport();
    if (wasPlaying) {
        emit playingChanged(false);
    }
}

void TimelineWidget::onLoopToggled(bool loop) {
    if (document_ != nullptr) {
        document_->setLoop(loop);
    }
}

void TimelineWidget::onOnionToggled(bool enabled) {
    if (canvas_ != nullptr) {
        canvas_->setOnionSkinEnabled(enabled);
    }
}

void TimelineWidget::onAddLayer() {
    if (document_ == nullptr) {
        return;
    }
    const size_t before = document_->document().layers.size();
    if (!document_->addLayer(tr("Layer %1").arg(before + 1))) {
        return;
    }
    // Select the new layer (always inserted on top, row 0).
    selectedRow_ = 0;
    refreshAll();
}

void TimelineWidget::onDeleteLayer() {
    if (document_ == nullptr) {
        return;
    }
    const uint64_t id = currentLayerId();
    if (id == 0) {
        return;
    }
    if (document_->document().layers.size() <= 1) {
        if (canvas_ != nullptr) {
            canvas_->reportStatus(tr("A document needs at least one layer."));
        }
        return;
    }
    document_->deleteLayer(id);
    selectedRow_ = std::max(0, selectedRow_ - (selectedRow_ > 0 ? 1 : 0));
    refreshAll();
}

void TimelineWidget::onPlaybackTick() {
    if (!playing_ || document_ == nullptr) {
        return;
    }
    const int end =
        hasWorkArea() ? workEnd_ : document_->document().FrameCount();
    const int start = hasWorkArea() ? workStart_ : 1;
    const int next = playhead_ + 1;
    if (next > end) {
        if (document_->document().loop) {
            setPlayhead(start);
        } else {
            onStop();
        }
        return;
    }
    setPlayhead(next);
}

void TimelineWidget::onHScroll(int value) {
    firstFrame_ = std::max(1, value);
    grid_->update();
}

void TimelineWidget::onVScroll(int value) {
    firstRow_ = std::max(0, value);
    grid_->update();
}

void TimelineWidget::selectRow(int row) {
    selectedRow_ = std::max(0, row);
    refreshAll();
}

void TimelineWidget::setFirstFrame(int frame) {
    firstFrame_ = std::max(1, frame);
    hScroll_->setValue(firstFrame_);
    grid_->update();
}

void TimelineWidget::setFirstRow(int row) {
    firstRow_ = std::max(0, row);
    vScroll_->setValue(firstRow_);
    grid_->update();
}

void TimelineWidget::beginScrub(int frame) {
    setPlayhead(frame);
}

void TimelineWidget::scrubTo(int frame) {
    setPlayhead(frame);
}

void TimelineWidget::endScrub() {
}

void TimelineWidget::beginWorkArea(int frame) {
    workStart_ = frame;
    workEnd_ = frame;
    grid_->update();
}

void TimelineWidget::extendWorkArea(int frame) {
    if (workStart_ < 1) {
        workStart_ = frame;
    }
    workEnd_ = frame;
    if (workEnd_ < workStart_) {
        std::swap(workStart_, workEnd_);
    }
    grid_->update();
}

void TimelineWidget::clearWorkArea() {
    if (!hasWorkArea()) {
        return;
    }
    workStart_ = -1;
    workEnd_ = -1;
    grid_->update();
}

void TimelineWidget::insertBlankAt(int row, int frame) {
    if (document_ == nullptr) {
        return;
    }
    const uint64_t layerId = layerIdAtRow(row);
    if (layerId == 0) {
        return;
    }
    const AnimLayer* layer = document_->document().FindLayerById(layerId);
    if (layer == nullptr || layer->Find(frame) != nullptr) {
        return;
    }
    if (frame > document_->document().FrameCount()) {
        return;
    }
    document_->insertKeyframe(layerId, frame, true);
    setPlayhead(frame);
}

void TimelineWidget::copyFrames(int row, int frame) {
    clipboard_.clear();
    if (document_ == nullptr) {
        return;
    }
    const AnimLayer* layer =
        document_->document().FindLayerById(layerIdAtRow(row));
    if (layer == nullptr) {
        return;
    }
    // M4 copies the single keyframe at the playhead; ranges come later.
    const AnimKeyframe* key = layer->Find(frame);
    if (key == nullptr) {
        if (canvas_ != nullptr) {
            canvas_->reportStatus(tr("Nothing to copy at this frame."));
        }
        return;
    }
    clipboard_.push_back(*key);
    if (canvas_ != nullptr) {
        canvas_->reportStatus(tr("Copied 1 keyframe."));
    }
}

void TimelineWidget::pasteFrames(int row, int frame) {
    if (document_ == nullptr || clipboard_.empty()) {
        return;
    }
    const uint64_t layerId = layerIdAtRow(row);
    if (layerId == 0) {
        return;
    }
    document_->pasteFrames(layerId, frame, clipboard_);
    setPlayhead(frame);
}
