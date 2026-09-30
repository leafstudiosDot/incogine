// Incogine Studio — main window (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#pragma once

#include <QMainWindow>
#include <QMap>
#include <QModelIndex>

#include <string>

class QFileSystemModel;
class QAction;
class QCloseEvent;
class QPlainTextEdit;
class QTreeView;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidget;
class QLineEdit;
class QTabWidget;

class CodeEditor;
class SearchPanel;
class PreviewSession;
class PreviewViewport;
class SceneEditorTab;

namespace icg {
namespace studio {
struct CreditsXml;
struct ProjectXml;
}
} // namespace icg

class StudioMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit StudioMainWindow(const std::string& projectRoot, QWidget* parent = nullptr);

public slots:
    void onOpenFile(const QString& path, int line = -1);

public:
    // Introspection helpers (used by --self-test).
    int filePageCount() const { return filePages_.size(); }
    QString currentTabTitle() const;

private slots:
    void onSaveCurrentFile();
    void onSaveAllFiles();
    void onRefreshProject();
    void onDiscoverScenes();
    void onSceneSelected(QTreeWidgetItem* item);
    void onCreditsSelected();
    void onSaveCredits();
    void onAddPerson();
    void onRemovePerson();
    void onSaveProjectSettings();
    void onPackIncoba();
    void onTreeContextMenu(const QPoint& pos);
    void onNewFile();
    void onNewFolder();
    void onRenameItem();
    void onDeleteItems();
    void onFocusSearch();
    void onTabCloseRequested(int index);
    void onSceneDirtyChanged(bool dirty);
    void onPreviewConsole(const QString& text);
    void cycleTab(int direction);
    void log(const QString& msg);

private:
    void buildMenus();
    void buildDocks();
    void buildCentral();
    void buildCreditsTab();
    void buildSettingsTab();
    void buildViewportTab();
    void closeEvent(QCloseEvent* event) override;
    bool restoreGeometryFromSettings();

    // VS Code-style tabs: one closable page per open file (title = path
    // relative to the project root, last 120 chars), pinned unclosable
    // Credits / Project Settings / Scene tabs, dirty (*) markers.
    bool isPinnedTab(QWidget* page) const;
    int firstPinnedTab() const;
    void showLoadingOverlay(QTreeView* view, QFileSystemModel* model,
                            const QString& root);
    QString tabTitleFor(const QString& path) const;
    QString baseTitleFor(const QString& kind, const QString& path) const;
    void refreshPinnedCloseButtons();
    void updatePageTitle(QWidget* page);
    CodeEditor* currentCodeEditor() const;
    CodeEditor* openCodePage(const QString& path, int line);
    QWidget* openAudioPage(const QString& path);
    QWidget* openFontPage(const QString& path);
    bool saveCodePage(CodeEditor* editor, const QString& path);
    bool closePage(QWidget* page); // false = user cancelled
    void refreshEditActions(); // Undo/Redo target the current code page

    std::string projectRoot_;
    QFileSystemModel* projectModel_ = nullptr;
    QFileSystemModel* assetsModel_ = nullptr;
    QTreeView* projectView_ = nullptr;
    QTreeView* assetsView_ = nullptr;
    QModelIndex contextIndex_;
    QFileSystemModel* contextModel_ = nullptr;
    QTreeWidget* sceneTree_ = nullptr;
    QPlainTextEdit* output_ = nullptr;
    QPlainTextEdit* previewConsole_ = nullptr;
    QTabWidget* central_ = nullptr;
    QAction* undoAction_ = nullptr; // code pages (per-tab document history)
    QAction* redoAction_ = nullptr;
    QWidget* sceneTab_ = nullptr;
    QString sceneTabBaseTitle_ = "Scene";
    QString selectedSceneClass_;
    PreviewSession* session_ = nullptr;
    SceneEditorTab* sceneEditor_ = nullptr;
    PreviewViewport* previewTab_ = nullptr;
    SearchPanel* searchPanel_ = nullptr;
    QDockWidget* searchDock_ = nullptr;
    // Open file pages by canonical path (code/audio/font widgets).
    QMap<QString, QWidget*> filePages_;

    // Credits tab widgets.
    QListWidget* creditsList_ = nullptr;
    QLineEdit* personName_ = nullptr;
    QLineEdit* personRole_ = nullptr;
    QPlainTextEdit* creditsXmlView_ = nullptr;
    icg::studio::CreditsXml* credits_ = nullptr;

    // Project settings tab widgets.
    QLineEdit* fName_ = nullptr;
    QLineEdit* fWindow_ = nullptr;
    QLineEdit* fId_ = nullptr;
    QLineEdit* fVersion_ = nullptr;
    QLineEdit* fDesc_ = nullptr;
    QLineEdit* fEngine_ = nullptr;
    QLineEdit* fAuthor_ = nullptr;
    QLineEdit* fCopyright_ = nullptr;
    QPlainTextEdit* fSettingsRaw_ = nullptr;
    QPlainTextEdit* settingsXmlView_ = nullptr;
    icg::studio::ProjectXml* project_ = nullptr;
};
