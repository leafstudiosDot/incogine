// Incogine Animator - Qt entry point.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// A separate top-level application, NOT a Studio child window: Incogine Studio
// launches it (double-clicking a `.incoanim` in the Asset Browser) and passes
// the file path, but closing Studio leaves an open animation running.
//
// Usage:
//   IncogineAnimator [<file.incoanim>]
//   IncogineAnimator --self-test [<file.incoanim>]
//
// --self-test builds the full window offscreen (QT_QPA_PLATFORM=offscreen),
// exercises open/edit/save/undo through the real command stack, and exits
// non-zero if anything is off - the same smoke-test shape Studio uses.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStyleFactory>
#include <QTimer>

#include <cmath>

#include <iostream>
#include <memory>

#include "canvas.h"
#include "animation/anim_io.h"
#include "channel.h"
#include "document.h"
#include "framecache.h"
#include "timeline.h"
#include "window.h"

namespace {

// Drives the window through a real open/edit/save/undo cycle and reports
// whether each step behaved. Phase 1 runs synchronously (everything except
// worker delivery); the worker check runs as phase 2 on the event loop (see
// the cache block below), which then quits with the exit code. Split because
// delivery only happens through real dispatch - polling it with a manual pump
// raced and hung the suite.
void runSelfTest(AnimatorWindow& window, const QString& path,
                 std::shared_ptr<int> exitCode) {
    auto failures = std::make_shared<int>(0);
    auto check = [failures](bool ok, const char* what) {
        std::cout << "self-test: " << what << (ok ? " ok" : " FAILED") << std::endl;
        if (!ok) {
            ++(*failures);
        }
    };

    AnimatorDocument* doc = window.controller();

    if (!path.isEmpty() && QFileInfo(path).exists()) {
        // Load through the controller, not window.openPath(): the window's
        // version raises a modal error box on failure, and a self-test must
        // never block waiting for a human.
        QString error;
        check(doc->load(path, &error), "loaded the animation passed on the command line");
        if (!error.isEmpty()) {
            std::cerr << "self-test: load error: " << error.toStdString() << std::endl;
        }
        check(!window.isDirty(), "opened document is clean");
        check(window.documentName() == QFileInfo(path).fileName(),
              "window title uses the file name");
        check(doc->hasPath(), "document has a path");
    } else {
        check(!window.isDirty(), "new document is clean");
        check(window.documentName() == QObject::tr("Untitled"),
              "new document is named Untitled");
    }

    // An edit through the command stack must mark dirty and create undo
    // history - this is the path the properties dock uses.
    const int fpsBefore = doc->document().fps;
    check(doc->setFps(fpsBefore == 12 ? 24 : 12), "edit applied");
    check(doc->isDirty(), "edit marks dirty");
    check(doc->canUndo(), "edit is undoable");

    // Undo moves the entry onto the redo stack, so redo becomes AVAILABLE
    // (not unavailable) afterwards.
    check(doc->undo(), "undo succeeds");
    check(doc->canRedo(), "undo makes the edit redoable");
    check(!doc->canUndo(), "undo empties the undo stack");
    check(doc->document().fps == fpsBefore, "undo restored the fps");
    check(doc->redo(), "redo succeeds");
    check(doc->canUndo(), "redo restores undo availability");
    check(doc->document().fps != fpsBefore, "redo re-applied the fps");

    // A no-op edit is refused rather than pushing dead history.
    const size_t depthBefore = doc->stack().undoDepth();
    check(!doc->setFps(doc->document().fps), "no-op edit refused");
    check(doc->stack().undoDepth() == depthBefore,
          "no-op edit leaves history untouched");

    // Layer commands drive through the same stack.
    const size_t layersBefore = doc->document().layers.size();
    check(doc->addLayer(QStringLiteral("Self Test Layer")), "add layer");
    check(doc->document().layers.size() == layersBefore + 1, "layer count grew");
    check(doc->undo(), "undo add layer");
    check(doc->document().layers.size() == layersBefore, "undo restored layers");

    // Stage size is reflected in the rendered stage (the placeholder canvas
    // paints the outline), so a repaint proves the view follows the model.
    check(doc->setStageSize(640, 360), "stage resize applied");
    check(doc->document().stageWidth == 640, "stage width applied");
    check(doc->undo(), "undo stage resize");

    std::cout << "self-test: document '" << window.documentName().toStdString()
              << "' dirty=" << (doc->isDirty() ? "yes" : "no")
              << " undoDepth=" << doc->stack().undoDepth() << std::endl;

    // Save-through-the-controller round-trip: a real write plus reload, so the
    // editor's file handling is covered, not just the engine's.
    const QString tempPath = QDir::temp().filePath(
        QStringLiteral("incoanim_selftest.incoanim"));
    QFile::remove(tempPath);
    doc->setStageSize(1280, 720);
    QString error;
    check(doc->saveAs(tempPath, &error), "saveAs wrote the file");
    check(QFileInfo::exists(tempPath), "file exists after save");
    check(!doc->isDirty(), "save clears the dirty flag");
    doc->reset();
    check(doc->load(tempPath, &error), "load reopened the saved file");
    check(doc->document().stageWidth == 1280, "reloaded stage width");
    check(doc->document().layers.size() == 1, "reloaded layer survived");

    // ---- canvas: view transform, hit testing, selection, editing -------
    // The canvas checks need artwork. When launched with no file (a bare
    // "Untitled" document) there is nothing to draw, so plant a fixture shape
    // directly in the model. Deliberately NOT a command: this is setup, and
    // pushing history here would skew the undo-depth checks below.
    AnimatorCanvas* canvas = window.findChild<AnimatorCanvas*>();
    if (canvas == nullptr) {
        check(false, "canvas widget found");
    } else {
        check(true, "canvas widget found");

        // A brand-new or just-reloaded document has a layer but no keyframe, so
        // there is nothing for the canvas to resolve or hit-test against. Plant
        // a keyframe holding one filled+stroked ellipse. Deliberately NOT done
        // through commands: this is fixture setup, and pushing history here would
        // skew the undo-depth checks below.
        if (canvas->activeKeyframe() == nullptr ||
            canvas->activeKeyframe()->shapes.empty()) {
            icg::anim::AnimDocument& model = doc->document();
            icg::anim::AnimLayer* layer =
                model.FindLayerById(canvas->activeLayerId());
            if (layer != nullptr) {
                icg::anim::AnimKeyframe seed;
                seed.frame = canvas->currentFrame();
                seed.kind = icg::anim::KeyframeKind::Key;
                layer->SetKeyframe(seed);

                icg::anim::AnimShape fixture;
                fixture.id = 4242;
                fixture.name = "fixture";
                fixture.path =
                    icg::anim::AnimPath::FromEllipse(0.0f, 0.0f, 200.0f, 150.0f);
                fixture.style.hasFill = true;
                fixture.style.fill = icg::anim::AnimColor(255, 128, 0, 200);
                fixture.style.hasStroke = true;
                fixture.style.stroke = icg::anim::AnimColor(0, 0, 0, 255);
                fixture.style.strokeWidth = 4.0f;
                if (icg::anim::AnimKeyframe* key = canvas->activeKeyframeMutable()) {
                    key->shapes.push_back(fixture);
                }
            }
        }
        check(canvas->activeKeyframe() != nullptr,
              "an active keyframe exists for the canvas checks");
        check(!canvas->drawList().empty(), "the canvas has drawable shapes");

        canvas->resize(1200, 800);
        QCoreApplication::processEvents();
        canvas->fitToStage();

        // The stage must actually paint: more than the flat backdrop, and with
        // the shape's orange fill on it.
        const QImage shot = canvas->grab().toImage();
        int bright = 0;
        int orange = 0;
        for (int y = 0; y < shot.height(); y += 2) {
            const QRgb* line =
                reinterpret_cast<const QRgb*>(shot.constScanLine(y));
            for (int x = 0; x < shot.width(); x += 2) {
                const QRgb p = line[x];
                if (qRed(p) + qGreen(p) + qBlue(p) > 200) {
                    ++bright;
                }
                if (qRed(p) > 180 && qGreen(p) > 80 && qGreen(p) < 180 &&
                    qBlue(p) < 90) {
                    ++orange;
                }
            }
        }
        check(bright > 0, "canvas painted the stage outline");
        check(orange > 0, "canvas painted the shape's fill color");

        // View transform round-trips stage <-> widget.
        const QPointF stagePoint(100.0, 50.0);
        const QPointF back = canvas->view().toStage(canvas->view().toWidget(stagePoint));
        check(std::fabs(back.x() - stagePoint.x()) < 1e-3 &&
                  std::fabs(back.y() - stagePoint.y()) < 1e-3,
              "view transform round-trips");
        check(canvas->view().zoom > 0.0f, "fit produced a positive zoom");

        // Hit testing: inside the demo blob, and well outside it.
        check(canvas->hitTest(QPointF(0.0, 0.0)) != 0,
              "hit test finds the shape at the stage origin");
        check(canvas->hitTest(QPointF(1279.0, 719.0)) == 0,
              "hit test misses at the far stage corner");

        // Selection.
        canvas->clearSelection();
        check(canvas->selectionCount() == 0, "selection starts empty");
        const uint64_t shapeId = canvas->drawList().empty() ? 0u : canvas->drawList().front().shapeId;
        canvas->setSelection(QSet<uint64_t>{shapeId});
        check(canvas->selectionCount() == 1, "selection takes a shape");
        canvas->toggleInSelection(shapeId);
        check(canvas->selectionCount() == 0, "ctrl-click toggles off");
        canvas->setSelection(QSet<uint64_t>{shapeId});

        // A drag moves every selected shape, and undo puts it back exactly.
        const float startX =
            doc->document().layers[0].FindMutable(1)->shapes[0]
                .transform.position.x;
        check(canvas->beginDrag(QPointF(100.0, 100.0)), "drag begins");
        canvas->updateDrag(QPointF(160.0, 130.0));
        const float movedX =
            doc->document().layers[0].FindMutable(1)->shapes[0]
                .transform.position.x;
        check(std::fabs(movedX - (startX + 60.0f)) < 0.01f,
              "drag applied a +60 stage-unit offset");
        // The cached draw list must follow the live drag, not paint stale.
        {
            bool found = false;
            for (const auto& resolved : canvas->drawList()) {
                if (resolved.shapeId == shapeId) {
                    found = true;
                    check(std::fabs(resolved.matrix.e - (startX + 60.0f)) <
                              0.05f,
                          "draw list follows the live drag");
                }
            }
            check(found, "dragged shape present in draw list");
        }
        check(canvas->commitDrag(), "drag committed");
        check(canvas->isDragging() == false, "drag state cleared");
        check(doc->undo(), "drag undo available");
        check(std::fabs(doc->document().layers[0].FindMutable(1)->shapes[0]
                            .transform.position.x -
                        startX) < 1e-3f,
              "undo restored the exact start position");
        check(doc->redo(), "drag redo available");

        // Marquee selects only FULLY enclosed shapes. The fixture is centred on the
        // stage origin, so it extends into negative coordinates - the enclosing
        // rect has to cover that, not just the stage.
        const auto enclosed = canvas->shapesInRect(QRectF(-400, -400, 800, 800));
        check(enclosed.size() == 1, "full-enclosure marquee takes the shape");
        check(canvas->shapesInRect(QRectF(400, 300, 50, 50)).empty(),
              "marquee away from the shape encloses nothing");

        // Delete, then undo restores it at its original index.
        const size_t shapesBefore = doc->document().layers[0].FindMutable(1)->shapes.size();
        canvas->setSelection(QSet<uint64_t>{shapeId});
        canvas->deleteSelection();
        check(doc->document().layers[0].FindMutable(1)->shapes.size() ==
                  shapesBefore - 1,
              "delete removed the shape");
        check(canvas->selectionCount() == 0, "delete cleared the selection");
        check(doc->undo(), "delete undo available");
        check(doc->document().layers[0].FindMutable(1)->shapes.size() ==
                  shapesBefore,
              "undo restored the shape");

        // A locked layer must refuse editing while staying inspectable.
        doc->document().layers[0].locked = true;
        check(canvas->isEditable() == false, "locked layer is not editable");
        doc->document().layers[0].locked = false;
        check(canvas->isEditable(), "unlocked layer is editable");

        // Culling round-trip: zoomed in the artwork must still paint (nothing
        // visible may be skipped); panned fully away it must vanish without
        // taking the app down; restored, every pixel comes back.
        {
            auto countBright = [&](int threshold) {
                const QImage image = canvas->grab().toImage();
                int n = 0;
                for (int y = 0; y < image.height(); y += 2) {
                    const QRgb* line =
                        reinterpret_cast<const QRgb*>(image.constScanLine(y));
                    for (int x = 0; x < image.width(); x += 2) {
                        if (qRed(line[x]) + qGreen(line[x]) + qBlue(line[x]) >
                            threshold) {
                            ++n;
                        }
                    }
                }
                return n;
            };
            canvas->fitToStage();
            check(countBright(200) > 0, "fit view paints");
            canvas->zoomBy(4.0);
            check(countBright(200) > 0, "zoomed view still paints");
            canvas->panBy(QPointF(20000.0, 20000.0));
            check(countBright(200) == 0, "panned-away view paints nothing");
            canvas->fitToStage();
            check(countBright(200) > 0, "artwork intact after cull round-trip");
        }

        // Tool dispatch.
        check(canvas->activeTool() != nullptr, "a tool is active by default");
        check(canvas->toolSet() != nullptr &&
                  canvas->toolSet()->all().size() == 4,
              "tool set holds Cursor, Hand, Brush, Pen");
        check(canvas->handTool() != nullptr, "hand tool available for Space");
        check(canvas->toolSet()->find("brush") != nullptr, "brush tool registered");
        check(canvas->toolSet()->find("pen") != nullptr, "pen tool registered");
        check(canvas->toolSet()->find("brush")->keyShortcut() == "B",
              "brush shortcut is B");
        check(canvas->toolSet()->find("pen")->keyShortcut() == "P",
              "pen shortcut is P");

        // Drawing options round-trip through the canvas.
        canvas->setStrokeWidth(7.5f);
        check(canvas->drawingOptions().strokeWidth == 7.5f, "stroke width set");
        canvas->setStrokeOpacity(0.5f);
        check(canvas->drawingOptions().opacity == 0.5f, "opacity set");
        canvas->setSmoothing(2.5f);
        check(canvas->drawingOptions().smoothing == 2.5f, "smoothing set");

        // Preview quality round-trips and moves the flatten tolerance.
        // Switching is view-only: the serialized project must not change.
        const std::string beforeQuality =
            icg::anim::Serialize(doc->document());
        canvas->setPreviewQuality(PreviewQuality::Draft);
        check(canvas->previewQuality() == PreviewQuality::Draft,
              "draft quality set");
        check(canvas->flattenTolerance() > 0.25f, "draft coarsens subdivision");
        canvas->setPreviewQuality(PreviewQuality::High);
        check(canvas->previewQuality() == PreviewQuality::High,
              "high quality set");
        canvas->setPreviewQuality(PreviewQuality::Low);
        check(canvas->previewQuality() == PreviewQuality::Low,
              "low quality restored");
        check(icg::anim::Serialize(doc->document()) == beforeQuality,
              "quality switches leave project data untouched");

        // Timeline: widget exists, playhead drives the canvas frame, layer
        // ops through the controller reflect in row count, onion toggles.
        {
            TimelineWidget* timeline = window.findChild<TimelineWidget*>();
            check(timeline != nullptr, "timeline widget found");
            if (timeline != nullptr) {
                check(timeline->rowCount() >= 1, "timeline lists layers");
                timeline->setPlayhead(3);
                check(timeline->playhead() == 1 || timeline->playhead() == 3,
                      "playhead clamps to the timeline length");
                check(canvas->currentFrame() == timeline->playhead(),
                      "playhead drives the canvas frame");
                const int rowsBefore = timeline->rowCount();
                check(doc->addLayer("Timeline Test"),
                      "timeline layer add commits");
                check(timeline->rowCount() == rowsBefore + 1,
                      "timeline row appears");
                check(doc->undo(), "timeline layer add undoes");
                check(timeline->rowCount() == rowsBefore,
                      "timeline row removed by undo");
                const uint64_t layerId = canvas->activeLayerId();
                check(doc->setLengthFrames(8), "timeline length extends");
                timeline->setPlayhead(3);
                check(timeline->playhead() == 3, "playhead moves in range");
                check(doc->insertKeyframe(layerId, 3, true),
                      "timeline blank keyframe inserts");
                check(doc->clearKeyframe(layerId, 3),
                      "timeline keyframe clears");
                check(doc->undo(), "timeline clear undoes");
                canvas->setOnionSkinEnabled(true);
                check(canvas->onionSkinEnabled(), "onion skin toggles on");
                canvas->setOnionSkinEnabled(false);
                check(!canvas->onionSkinEnabled(), "onion skin toggles off");
                // Playback interplay through the real user path: Space on the
                // grid starts the timer, an FPS edit mid-play retimes the
                // ticks, Space stops. The timer is always stopped before the
                // test moves on, so nothing lingers.
                TimelineGrid* grid = window.findChild<TimelineGrid*>();
                check(grid != nullptr, "timeline grid found");
                if (grid != nullptr) {
                    const auto expectedInterval = [](int fps) {
                        return std::max(1, 1000 / std::max(1, fps));
                    };
                    QKeyEvent space(QEvent::KeyPress, Qt::Key_Space,
                                    Qt::NoModifier);
                    QApplication::sendEvent(grid, &space);
                    check(timeline->isPlaying(), "space starts playback");
                    check(timeline->playbackIntervalMs() ==
                              expectedInterval(doc->document().fps),
                          "playback ticks at document fps");
                    check(canvas->isPlaybackActive(),
                          "playback engages the cache blit");
                    // A value that differs: the early fps test leaves 12.
                    const int newFps =
                        doc->document().fps == 12 ? 24 : 12;
                    check(doc->setFps(newFps), "fps edits mid-playback");
                    check(timeline->playbackIntervalMs() ==
                              expectedInterval(newFps),
                          "fps edit retimes playback ticks");
                    check(timeline->isPlaying(),
                          "retime does not stop playback");
                    QApplication::sendEvent(grid, &space);
                    check(!timeline->isPlaying(), "space pauses playback");
                    check(!canvas->isPlaybackActive(),
                          "pause releases the cache blit");
                    check(doc->undo(), "fps edit undoes");
                }
                timeline->setPlayhead(1);
            }
        }

        // A drawn shape commits through the stack, selects itself, and undoes.
        {
            icg::anim::AnimPath path = icg::anim::AnimPath::FromRect(
                500.0f, 400.0f, 60.0f, 30.0f);
            icg::anim::AnimStyle style;
            style.hasFill = false;
            style.hasStroke = true;
            style.stroke = icg::anim::AnimColor(10, 20, 30, 40);
            style.strokeWidth = 7.5f;
            const uint64_t layerId = canvas->activeLayerId();
            const int frame = canvas->currentFrame();
            const size_t before =
                canvas->activeKeyframe()->shapes.size();
            const uint64_t drawnId =
                canvas->addDrawnShape(std::move(path), style, "Self Test Stroke");
            check(drawnId != 0, "drawn shape committed");
            check(canvas->activeKeyframe()->shapes.size() == before + 1,
                  "drawn shape appended on top");
            check(canvas->isSelected(drawnId), "drawn shape auto-selected");
            check(canvas->hitTest(QPointF(505.0, 400.0)) != 0,
                  "drawn stroke is hit-testable");
            check(doc->undo(), "draw undo available");
            check(canvas->activeKeyframe()->shapes.size() == before,
                  "draw undo removed the shape");
            check(doc->redo(), "draw redo available");
        }

        // A locked layer refuses new shapes without touching the stack.
        {
            const size_t depthBefore = doc->stack().undoDepth();
            doc->document().layers[0].locked = true;
            icg::anim::AnimPath path = icg::anim::AnimPath::FromRect(
                0.0f, 0.0f, 10.0f, 10.0f);
            icg::anim::AnimStyle style;
            style.hasStroke = true;
            check(canvas->addDrawnShape(std::move(path), style, "Blocked") == 0,
                  "locked layer refuses new shapes");
            check(doc->stack().undoDepth() == depthBefore,
                  "refused draw leaves no history");
            doc->document().layers[0].locked = false;
        }

        // The real Brush tool is a freeform pen: synthetic events drive a
        // stroke whose committed shape must be a compact centerline + width.
        // Pixel counts before/after/undo/redo prove the scene cache bakes the
        // commit incrementally and erases it exactly on undo (regional
        // rebake), not approximately.
        //
        // Pin the geometry first: the window layout can shrink the canvas
        // (measured 240x254 once), which silently moves every synthetic point
        // off-stage and off-screen and makes pixel counts meaningless. All
        // widget coordinates below assume this size and fit.
        {
            canvas->resize(1200, 800);
            canvas->fitToStage();
            canvas->setActiveTool("brush");
            canvas->setStrokeWidth(6.0f);
            canvas->setSmoothing(1.0f);
            ITool* brush = canvas->toolSet()->find("brush");
            check(brush != nullptr, "brush tool found for stroke test");
            // Anything clearly non-background: the stroke paints translucent
            // black (opacity 0.5 over the checker), so a near-black threshold
            // would miss it. The baseline absorbs the checker either way; only
            // the before/after/undo deltas matter.
            auto countDark = [&]() {
                const QImage image = canvas->grab().toImage();
                int n = 0;
                for (int y = 0; y < image.height(); y += 2) {
                    const QRgb* line =
                        reinterpret_cast<const QRgb*>(image.constScanLine(y));
                    for (int x = 0; x < image.width(); x += 2) {
                        if (qRed(line[x]) + qGreen(line[x]) + qBlue(line[x]) <
                            600) {
                            ++n;
                        }
                    }
                }
                return n;
            };
            // No highlight during pixel comparisons: the blue selection trace
            // would confound the bake-exactness deltas below.
            canvas->clearSelection();
            // Hover first so the baseline includes the brush ring: hasHover_
            // flips on the first move and the ring then paints in every grab.
            // Without this, the ring's pixels would masquerade as bake residue
            // in the undo comparison below.
            brush->onMove(
                *canvas,
                QMouseEvent(QEvent::MouseMove, QPointF(420.0, 308.0),
                            QPointF(420.0, 308.0), QPointF(420.0, 308.0),
                            Qt::NoButton, Qt::NoButton, Qt::NoModifier));
            auto countBlue = [&]() {
                const QImage image = canvas->grab().toImage();
                int n = 0;
                for (int y = 0; y < image.height(); y += 2) {
                    const QRgb* line =
                        reinterpret_cast<const QRgb*>(image.constScanLine(y));
                    for (int x = 0; x < image.width(); x += 2) {
                        if (qAbs(qRed(line[x]) - 60) +
                                qAbs(qGreen(line[x]) - 140) +
                                qAbs(qBlue(line[x]) - 255) <
                            90) {
                            ++n;
                        }
                    }
                }
                return n;
            };
            const int darkBefore = countDark();
            const int blueBefore = countBlue();
            const size_t before =
                canvas->activeKeyframe()->shapes.size();
            const QPointF p0(300.0, 300.0);
            check(brush->onPress(
                      *canvas,
                      QMouseEvent(QEvent::MouseButtonPress, p0, p0, p0,
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier)),
                  "brush press starts a stroke");
            for (int i = 1; i <= 10; ++i) {
                const QPointF p(p0.x() + i * 12.0,
                                p0.y() + std::sin(i * 0.9) * 20.0);
                brush->onMove(
                    *canvas,
                    QMouseEvent(QEvent::MouseMove, p, p, p, Qt::NoButton,
                                Qt::LeftButton, Qt::NoModifier));
            }
            const QPointF pEnd(p0.x() + 132.0, p0.y());
            check(brush->onRelease(
                      *canvas,
                      QMouseEvent(QEvent::MouseButtonRelease, pEnd, pEnd, pEnd,
                                  Qt::LeftButton, Qt::NoButton, Qt::NoModifier)),
                  "brush release commits a shape");
            check(canvas->activeKeyframe()->shapes.size() == before + 1,
                  "brush stroke appended one shape");
            const icg::anim::AnimShape& committed =
                canvas->activeKeyframe()->shapes.back();
            // Compact source storage: a centerline + width, not tessellation.
            check(committed.style.hasStroke &&
                      !committed.style.hasFill,
                  "brush shape is a stroked centerline, not baked fill");
            check(committed.path.segments.size() < 100,
                  "brush shape stores dozens of segments, not thousands");
            // Probe ON the stroke: widget point of move 5, which the fitted
            // centerline passes within the smoothing tolerance of, well
            // inside the half width.
            const QPointF pMid(p0.x() + 5 * 12.0,
                               p0.y() + std::sin(5 * 0.9) * 20.0);
            check(canvas->hitTest(canvas->view().toStage(pMid)) != 0,
                  "brush shape is hit-testable on its band");
            check(canvas->isSelected(committed.id),
                  "brush shape auto-selected");
            const int darkCommitted = countDark();
            const int selCommitted = canvas->selectionCount();
            const int blueCommitted = countBlue();
            check(darkCommitted > darkBefore + 100,
                  "committed stroke paints pixels through the scene cache");
            check(blueCommitted > blueBefore,
                  "selection highlight paints its trace");
            // De-highlight so the undo/redo comparisons below measure only
            // baked pixels, never selection-trace pixels.
            canvas->clearSelection();
            const int darkPlain = countDark();
            const int selPlain = canvas->selectionCount();
            check(doc->undo(), "brush undo available");
            check(canvas->activeKeyframe()->shapes.size() == before,
                  "brush undo removed the shape");
            const int darkUndone = countDark();
            check(darkUndone >= darkBefore - 2 && darkUndone <= darkBefore + 2,
                  "undo regional rebake restores pre-stroke pixels exactly");
            check(doc->redo(), "brush redo available");
            const int darkRedone = countDark();
            check(darkRedone >= darkPlain - 2 && darkRedone <= darkPlain + 2,
                  "redo rebakes the stroke's pixels exactly");
        }
        // RAM frame cache (1.3): sync bake size, precise invalidation,
        // compositing-only survival, budget eviction, worker prefetch.
        {
            FrameCache* cache = window.findChild<FrameCache*>();
            check(cache != nullptr, "frame cache owned by the window");
            TimelineWidget* timeline =
                window.findChild<TimelineWidget*>();
            check(timeline != nullptr &&
                      timeline->frameCache() == cache,
                  "timeline strip reads the shared cache");
            if (cache != nullptr) {
                const uint64_t layerId = canvas->activeLayerId();
                check(doc->document().FrameCount() >= 6,
                      "cache fixture has frames to span");
                // Sync bake: stage-sized, and non-empty where art resolves.
                const QImage baked = cache->composite(2);
                check(!baked.isNull(), "cache bakes a composite");
                check(baked.width() == doc->document().stageWidth &&
                          baked.height() == doc->document().stageHeight,
                      "cached composite is stage-sized");
                int opaque = 0;
                for (int y = 0; y < baked.height(); y += 4) {
                    const QRgb* line = reinterpret_cast<const QRgb*>(
                        baked.constScanLine(y));
                    for (int x = 0; x < baked.width(); x += 4) {
                        if (qAlpha(line[x]) > 0) {
                            ++opaque;
                        }
                    }
                }
                check(opaque > 0, "cached composite holds artwork pixels");
                check(cache->hasComposite(2), "frame 2 composite cached");
                // Precise invalidation: an edit at frame 6 drops its own
                // span only - frame 2 survives, with its layer pixels intact.
                cache->composite(6);
                check(cache->hasComposite(6), "frame 6 composite cached");
                check(cache->hasLayer(layerId, 2),
                      "frame 2 layer image cached");
                check(doc->insertKeyframe(layerId, 6, false),
                      "keyframe inserts at 6");
                check(!cache->hasComposite(6),
                      "edit drops its own span");
                check(cache->hasComposite(2),
                      "distant frame survives the edit");
                check(cache->hasLayer(layerId, 2),
                      "distant layer pixels survive the edit");
                check(doc->undo(), "keyframe insert undoes");
                check(cache->hasComposite(2),
                      "undo keeps the distant frame");
                // Compositing-only edits (visibility) drop assembled frames
                // but keep layer pixels: the next composite reassembles with
                // zero vector work.
                cache->composite(2);
                check(doc->setLayerVisible(layerId, false),
                      "layer hides");
                check(!cache->hasComposite(2),
                      "visibility drops assembled frames");
                check(cache->hasLayer(layerId, 2),
                      "visibility keeps layer pixels");
                check(doc->setLayerVisible(layerId, true),
                      "layer shows again");
                // Budget eviction: small budget holds, playhead frame wins.
                cache->clear();
                cache->setBudgetBytes(8LL * 1024LL * 1024LL);
                cache->setPlayhead(4);
                cache->composite(1);
                cache->composite(2);
                cache->composite(3);
                cache->composite(4);
                check(cache->bytesUsed() <= cache->budgetBytes(),
                      "cache honors its byte budget");
                check(cache->hasComposite(4),
                      "playhead frame survives eviction");
                cache->setBudgetBytes(256LL * 1024LL * 1024LL);
                // Worker prefetch: frames ahead bake off-thread. Delivery is
                // verified in phase 2 (below), through real event-loop
                // dispatch - never by polling with a manual pump, which
                // proved flaky (a manual pump raced worker delivery and hung
                // the suite intermittently with the thread parked at 0% CPU).
                // Phase 1 ends here: it returns so the loop runs freely
                // (worker bakes, paints paint, timers fire - exactly like
                // production playback), and phase 2 asserts afterwards.
                cache->clear();
                cache->prefetchAround(1);
                // Phase 2 runs after the loop has delivered worker results
                // through real dispatch (see above): asserts delivery,
                // finishes the suite, quits the loop with the exit code.
                // Captures are all shared or long-lived (the window, canvas,
                // and cache outlive the loop; failures/exitCode are shared),
                // so running after phase 1 returns is safe. The delay is
                // generous on purpose: Debug rasterizes full-stage frames
                // slowly, and frame 3 waits behind heavier frames in FIFO.
                QTimer::singleShot(10000, QApplication::instance(),
                                   [check, failures, exitCode, tempPath,
                                    canvas, cache] {
                                       check(cache->hasComposite(3),
                                             "worker prefetches frames ahead");
                                       canvas->setPlaybackActive(true);
                                       check(canvas->isPlaybackActive(),
                                             "playback mode engages");
                                       canvas->setPlaybackActive(false);
                                       check(!canvas->isPlaybackActive(),
                                             "playback mode releases");
                                       QFile::remove(tempPath);
                                       std::cout << "self-test: finished ("
                                                 << *failures << " failure(s))"
                                                 << std::endl;
                                       *exitCode = *failures == 0 ? 0 : 1;
                                       // Drain-then-quit: a bare quit() here
                                       // usually leaves the loop running
                                       // (thread parked at 0% CPU, never
                                       // exits); quitting after one pump has
                                       // exited cleanly but not reliably, so
                                       // exit stays flaky. Root cause inside
                                       // Qt's event delivery is unknown - the
                                       // kill-after-run workflow in the build
                                       // notes applies either way.
                                       QCoreApplication::processEvents();
                                       QApplication::instance()->quit();
                                   });
                return;
            }
        }
    }

    // Reached only when the window has no frame cache (never in production -
    // the window always wires one): finish inline so the suite still reports.
    QFile::remove(tempPath);

    std::cout << "self-test: finished (" << *failures << " failure(s))" << std::endl;
    *exitCode = *failures == 0 ? 0 : 1;
    // Drain-then-quit, as in phase 2 above.
    QCoreApplication::processEvents();
    QApplication::instance()->quit();
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Incogine Animator");
    app.setOrganizationName("leafstudiosDot");
    // Distinct from Studio's key so the two apps do not overwrite each other's
    // geometry memory.
    app.setApplicationDisplayName("Incogine Animator");

    bool selfTest = false;
    QString path;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--self-test")) {
            selfTest = true;
        } else if (arg.startsWith(QStringLiteral("--"))) {
            std::cerr << "unknown option: " << arg.toStdString() << "\n";
            return 2;
        } else if (path.isEmpty()) {
            path = arg;
        }
    }

    // A bad path from Studio must fail loudly instead of silently opening a new
    // document. In self-test mode there is nobody to dismiss a dialog, so report
    // on stderr and exit non-zero instead of blocking on a modal message box.
    if (!path.isEmpty() && !QFileInfo(path).exists()) {
        if (selfTest) {
            std::cerr << "self-test: no such animation file: "
                      << path.toStdString() << std::endl;
            return 1;
        }
        QMessageBox::critical(
            nullptr, "Incogine Animator",
            QObject::tr("No such animation file:\n%1").arg(path));
        return 1;
    }

    // Another Animator may already have this document open (Studio, a file
    // association, or a second launch). Hand the path over and exit rather than
    // opening a second editor on the same file, which would let two windows race
    // to save it. Studio performs the same check before spawning us, so this
    // only fires for direct launches.
    if (!path.isEmpty() && !selfTest && AnimatorChannel::handOffTo(path)) {
        std::cout << "Incogine Animator: handed off to the window already editing "
                  << QFileInfo(path).fileName().toStdString() << "\n";
        return 0;
    }
    AnimatorWindow window;
    if (!path.isEmpty() && !selfTest) {
        window.openPath(path);
    }

    if (!selfTest) {
        window.show();
        return app.exec();
    }

    // Self-test drives the window inside a real event loop, mirroring Studio's
    // --self-test: phase 1 runs in one timer; worker delivery is asserted in
    // a later phase (scheduled from phase 1), and the quit comes from phase 2
    // or the backstop below - never from inside the working callback, which
    // would tear the application down without the loop having run to
    // completion and crash during static destruction (the same reason Studio
    // schedules its quit separately). The backstop also bounds the suite:
    // nonzero exit when a phase stalls instead of lingering forever.
    std::cout << "self-test: entering self-test mode" << std::endl;
    // Dark palette exercises the themed-paint path with a non-default style,
    // matching Studio's self-test.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette dark = app.palette();
    dark.setColor(QPalette::Window, QColor(53, 53, 53));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Base, QColor(25, 25, 25));
    dark.setColor(QPalette::Text, Qt::white);
    app.setPalette(dark);
    std::cout << "self-test: dark palette applied" << std::endl;

    window.show();

    // Default nonzero: if no phase ever reports (a stall), the backstop quit
    // below still ends the run and it reads as failure, not success.
    auto exitCode = std::make_shared<int>(1);
    QTimer::singleShot(0, &app, [&] { runSelfTest(window, path, exitCode); });
    QTimer::singleShot(180000, &app, &QApplication::quit);
    const int loopCode = app.exec();
    std::cout << "self-test: event loop exited" << std::endl;
    return *exitCode != 0 ? *exitCode : loopCode;
}

