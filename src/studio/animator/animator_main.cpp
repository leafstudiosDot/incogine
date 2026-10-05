// Incogine Animator — Qt entry point.
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
// non-zero if anything is off — the same smoke-test shape Studio uses.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QStyleFactory>
#include <QTimer>

#include <iostream>

#include "animator_channel.h"
#include "animator_document.h"
#include "animator_window.h"

namespace {

// Drives the window through a real open/edit/save/undo cycle and reports
// whether each step behaved. Returns a process exit code.
int runSelfTest(AnimatorWindow& window, const QString& path) {
    int failures = 0;
    auto check = [&failures](bool ok, const char* what) {
        std::cout << "self-test: " << what << (ok ? " ok" : " FAILED") << std::endl;
        if (!ok) {
            ++failures;
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
    // history — this is the path the properties dock uses.
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

    // The stage must actually paint more than background, proving the canvas
    // follows the model (same check shape Studio's self-test uses).
    QCoreApplication::processEvents();
    if (auto* stage = window.findChild<AnimatorStageView*>()) {
        const QImage shot = stage->grab().toImage();
        int bright = 0;
        for (int y = 0; y < shot.height(); y += 4) {
            const QRgb* line =
                reinterpret_cast<const QRgb*>(shot.constScanLine(y));
            for (int x = 0; x < shot.width(); x += 4) {
                if (qRed(line[x]) + qGreen(line[x]) + qBlue(line[x]) > 200) {
                    ++bright;
                }
            }
        }
        check(bright > 0, "stage canvas painted the stage outline");
    }
    QFile::remove(tempPath);

    std::cout << "self-test: finished (" << failures << " failure(s))" << std::endl;
    return failures == 0 ? 0 : 1;
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
    // --self-test: the work runs in one timer and the quit comes from a
    // SEPARATE later timer. Calling quit() from inside the working callback
    // leaves the application torn down without the loop having run to
    // completion, which crashes during static destruction — the same reason
    // Studio schedules its quit separately.
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

    int exitCode = 0;
    QTimer::singleShot(0, &app, [&] { exitCode = runSelfTest(window, path); });
    QTimer::singleShot(750, &app, &QApplication::quit);
    const int loopCode = app.exec();
    return exitCode != 0 ? exitCode : loopCode;
}