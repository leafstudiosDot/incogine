// Incogine Studio — Qt entry point.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QStyleFactory>
#include <QTimer>
#include <QTreeWidget>

#include <iostream>

#include "ui/main_window.h"
#include "ui/preview_viewport.h"
#include "ui/scene_editor.h"

#include "core/preview/preview_exe.h"
#include "core/project_paths.h"
#include "core/xml/project_xml.h"

namespace {

// Studio refuses to run unless the development game build verifies:
// executable located in a CMake build tree of this project and its
// SHA-256 matching the CMake-written sidecar. Foreign projects,
// released binaries, and swapped-in files all fail closed.
bool checkDevBinding(const std::string& root, std::string& errorOut) {
    icg::studio::ProjectXml project;
    std::string error;
    if (!icg::studio::ProjectXml::ParseFile(root + "/src/project.xml", project,
                                            error) ||
        project.name.empty()) {
        errorOut = "Cannot read project identity (src/project.xml): " + error;
        return false;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    icg::studio::preview::PreviewExeInfo info;
    if (!icg::studio::preview::LocatePreviewExe(
            root, project.name,
            {appDir.toStdString(), (appDir + "/..").toStdString(),
             (appDir + "/../..").toStdString()},
            info, error)) {
        errorOut = "No development build found: " + error +
                   "\nBuild the project with CMake first.";
        return false;
    }
    icg::studio::preview::PreviewBinding binding;
    if (!icg::studio::preview::VerifyPreviewExe(root, project.name, info.exePath,
                                                binding, error)) {
        errorOut = "Development build verification failed: " + error;
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Incogine Studio");
    app.setOrganizationName("leafstudiosDot");

    bool selfTest = false;
    std::string root = ".";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") {
            selfTest = true;
        } else if (!arg.empty() && arg[0] != '-') {
            root = arg;
        }
    }
    const auto found = icg::studio::ProjectPaths::FindRoot(root);
    if (!found.empty()) {
        root = found.string();
    } else if (!selfTest) {
        QMessageBox::warning(nullptr, "Incogine Studio",
                             "Could not find src/project.xml above the given directory.\n"
                             "File browsers will still open, but XML models will fail to load.");
    }

    if (!selfTest) {
        // Binding gate: Studio only opens against a verified development
        // build. Anything else force-closes with an error.
        std::string bindingError;
        if (!checkDevBinding(root, bindingError)) {
            std::cerr << "binding failed: " << bindingError << "\n";
            QMessageBox::critical(nullptr, "Incogine Studio",
                                  QString::fromStdString(bindingError));
            return 1;
        }
        std::cout << "binding ok for root " << root << "\n";
    } else {
        std::cout << "self-test: binding gate bypassed (diagnostic mode)\n";
    }

    StudioMainWindow win(root);
    win.show();
    if (selfTest) {
        // Headless smoke test (QT_QPA_PLATFORM=offscreen): everything
        // else runs for real.
        // Headless smoke test (QT_QPA_PLATFORM=offscreen): the constructor
        // already runs scene discovery + XML model loads; also flip through
        // a dark palette so the highlighters rebuild (paletteChanged path),
        // pump events once so deferred file-system/model work runs.
        std::cout << "self-test: window constructed for root " << root << "\n";
        win.onOpenFile(QString::fromStdString(root + "/src/project.xml"));
        win.onOpenFile(QString::fromStdString(root + "/src/assets/fonts/main_font.ttf"));
        win.onOpenFile(QString::fromStdString(root + "/src/assets/audio/testbgm.ogg"));
        std::cout << "self-test: code/font/audio pages opened\n";
        const int pagesBefore = win.filePageCount();
        if (auto* tree = win.findChild<QTreeWidget*>()) {
            if (QTreeWidgetItem* top = tree->topLevelItem(0)) {
                QMetaObject::invokeMethod(&win, "onSceneSelected",
                                          Q_ARG(QTreeWidgetItem*, top));
            }
        }
        std::cout << "self-test: scene selected, file pages "
                  << pagesBefore << " -> " << win.filePageCount()
                  << ", current tab '" << win.currentTabTitle().toStdString() << "'\n";
        // Scene sidebar selection drives the editor (class + parsed
        // hierarchy); the launch argument reuses the same class name.
        if (auto* editor = win.findChild<SceneEditorTab*>()) {
            std::cout << "self-test: editor scene='"
                      << editor->sceneClass().toStdString() << "' objects="
                      << editor->sceneObjectCount() << "\n";
        }
        app.setStyle(QStyleFactory::create("Fusion"));
        QPalette dark = app.palette();
        dark.setColor(QPalette::Window, QColor(53, 53, 53));
        dark.setColor(QPalette::WindowText, Qt::white);
        dark.setColor(QPalette::Base, QColor(25, 25, 25));
        dark.setColor(QPalette::Text, Qt::white);
        app.setPalette(dark);
        std::cout << "self-test: dark palette applied\n";
        QTimer::singleShot(400, &win, [&win] {
            int visibleOverlays = 0;
            for (auto* label : win.findChildren<QLabel*>("loadingOverlay")) {
                if (label->isVisible()) {
                    ++visibleOverlays;
                }
            }
            std::cout << "self-test: visible loading overlays " << visibleOverlays << "\n";
            // Offline scene render must paint more than background: grab the
            // editor canvas (Credits scene = border only) and count lit pixels.
            if (auto* canvas = win.findChild<SceneCanvas*>()) {
                const QImage shot = canvas->grab().toImage();
                int lit = 0, bright = 0;
                for (int y = 0; y < shot.height(); y += 4) {
                    const QRgb* line =
                        reinterpret_cast<const QRgb*>(shot.constScanLine(y));
                    for (int x = 0; x < shot.width(); x += 4) {
                        const QRgb p = line[x];
                        const int sum = qRed(p) + qGreen(p) + qBlue(p);
                        if (sum > 24) {
                            ++lit;
                        }
                        if (sum > 200) {
                            ++bright; // border / text / selection only
                        }
                    }
                }
                std::cout << "self-test: canvas lit pixels " << lit
                          << " bright " << bright
                          << (bright > 0 ? "" : " FAILED") << "\n";
            }
        });
        QTimer::singleShot(500, &app, &QApplication::quit);
    }
    const int code = app.exec();
    if (selfTest) {
        std::cout << "self-test: event loop exited (" << code << ")\n";
    }
    return code;
}
