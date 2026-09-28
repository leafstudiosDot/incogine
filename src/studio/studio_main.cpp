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

#include "core/project_paths.h"

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

    StudioMainWindow win(root);
    win.show();
    if (selfTest) {
        // Headless smoke test (QT_QPA_PLATFORM=offscreen): the constructor
        // already runs scene discovery + XML model loads; also flip through
        // a dark palette so the highlighters rebuild (paletteChanged path),
        // pump events once so deferred file-system/model work runs.
        std::cout << "self-test: window constructed for root " << root << "\n";
        win.onOpenFile(QString::fromStdString(root + "/src/project.xml"));
        win.onOpenFile(QString::fromStdString(root + "/src/assets/fonts/main_font.ttf"));
        win.onOpenFile(QString::fromStdString(root + "/src/assets/audio/testbgm.ogg"));
        std::cout << "self-test: code/font/audio pages opened\n";
        // Scene sidebar clicks must land on the Scene tab (renamed) without
        // opening extra file tabs.
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
        });
        QTimer::singleShot(500, &app, &QApplication::quit);
    }
    const int code = app.exec();
    if (selfTest) {
        std::cout << "self-test: event loop exited (" << code << ")\n";
    }
    return code;
}
