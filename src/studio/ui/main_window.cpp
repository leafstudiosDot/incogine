// Incogine Studio — main window implementation (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "main_window.h"

#include <QDockWidget>
#include <QDir>
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextStream>
#include <QToolBar>
#include <QTreeView>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "code_editor.h"
#include "preview_session.h"
#include "preview_viewport.h"
#include "scene_editor.h"
#include "search_panel.h"
#include "font_preview.h"
#include "preview_viewport.h"
#ifdef ICG_STUDIO_HAS_MULTIMEDIA
#include "audio_preview.h"
#endif
#include "../core/xml/credits_xml.h"
#include "../core/incoba/incoba.h"
#include "../core/project_paths.h"
#include "../core/xml/project_xml.h"
#include "../core/scene/scene_discovery.h"

StudioMainWindow::StudioMainWindow(const std::string& projectRoot, QWidget* parent)
    : QMainWindow(parent), projectRoot_(projectRoot) {
    credits_ = new icg::studio::CreditsXml();
    project_ = new icg::studio::ProjectXml();
    session_ = new PreviewSession(this);
    setWindowTitle(tr("Incogine Studio"));
    buildMenus();
    buildCentral();
    buildDocks();
    // Roomier than the engine's 1280x720 default on first run; thereafter
    // the saved size/position/dock layout is restored (see closeEvent).
    if (!restoreGeometryFromSettings()) {
        resize(1440, 900);
    }
    onDiscoverScenes();
    onRefreshProject();
    log(tr("Incogine Studio — project root: %1").arg(QString::fromStdString(projectRoot_)));
}

void StudioMainWindow::buildMenus() {
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("Save current file"), this, &StudioMainWindow::onSaveCurrentFile, QKeySequence::Save);
    file->addAction(tr("Save all files"), this, &StudioMainWindow::onSaveAllFiles);
    file->addSeparator();
    file->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = edit->addAction(tr("Undo"), this, [this] {
        if (auto* editor = currentCodeEditor()) {
            editor->editor()->undo();
        }
    });
    undoAction_->setShortcuts(QKeySequence::keyBindings(QKeySequence::Undo));
    undoAction_->setEnabled(false);
    redoAction_ = edit->addAction(tr("Redo"), this, [this] {
        if (auto* editor = currentCodeEditor()) {
            editor->editor()->redo();
        }
    });
    redoAction_->setShortcuts(QKeySequence::keyBindings(QKeySequence::Redo));
    redoAction_->setEnabled(false);
    edit->addSeparator();
    edit->addAction(tr("Find..."), this, [this] {
        if (auto* editor = currentCodeEditor()) {
            editor->openFind(false);
        }
    }, QKeySequence::Find);
    edit->addAction(tr("Replace..."), this, [this] {
        if (auto* editor = currentCodeEditor()) {
            editor->openFind(true);
        }
    }, QKeySequence::Replace);
    edit->addAction(tr("Find in files..."), this, &StudioMainWindow::onFocusSearch,
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(tr("Refresh project"), this, &StudioMainWindow::onRefreshProject, QKeySequence::Refresh);

    QMenu* project = menuBar()->addMenu(tr("&Project"));
    project->addAction(tr("Open project.xml settings tab"), this, [this] { central_->setCurrentIndex(2); });
    project->addAction(tr("Open credits.xml editor tab"), this, [this] { central_->setCurrentIndex(1); });
    project->addAction(tr("Find in files..."), this, &StudioMainWindow::onFocusSearch);
    project->addAction(tr("Pack assets into .incoba bundles..."), this, &StudioMainWindow::onPackIncoba);

    QMenu* scene = menuBar()->addMenu(tr("&Scene"));
    scene->addAction(tr("Rescan scenes"), this, &StudioMainWindow::onDiscoverScenes);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About Incogine Studio"), this, [this] {
        // Engine version comes from src/project.xml (<incogine_version>,
        // mirrored from src/core/engine/version.h) — never hardcoded.
        QString engineVersion = tr("unknown");
        icg::studio::ProjectXml projectXml;
        std::string error;
        if (icg::studio::ProjectXml::ParseFile(projectRoot_ + "/src/project.xml",
                                               projectXml, error) &&
            !projectXml.incogineVersion.empty()) {
            engineVersion = QString::fromStdString(projectXml.incogineVersion);
        }
        QMessageBox::about(this, tr("About Incogine Studio"),
                           tr("Incogine Studio for Incogine v%1 by leafstudiosDot")
                               .arg(engineVersion));
    });

    QToolBar* tb = addToolBar(tr("Main"));
    tb->addAction(tr("Save"), this, &StudioMainWindow::onSaveCurrentFile);
    tb->addAction(tr("Refresh"), this, &StudioMainWindow::onRefreshProject);
    tb->addAction(tr("Rescan scenes"), this, &StudioMainWindow::onDiscoverScenes);
    // Manual game compile only (never Studio itself, never automatic):
    // builds the CMake game target so scene/source edits can be tested.
    compileAction_ = tb->addAction(tr("Compile Incogine"), this,
                                   &StudioMainWindow::onCompileGame);
    compileAction_->setToolTip(
        tr("Configure (first run) and build the Incogine target with CMake"));
}

void StudioMainWindow::buildCentral() {
    central_ = new QTabWidget(this);
    central_->setTabsClosable(true);
    central_->setMovable(true);
    central_->setElideMode(Qt::ElideRight);
    central_->tabBar()->setUsesScrollButtons(true);
    // Notepad++-style paging: tabs keep a readable minimum width and scroll
    // instead of squeezing down to a single character.
    central_->tabBar()->setStyleSheet("QTabBar::tab { min-width: 110px; }");
    setCentralWidget(central_);
    connect(central_, &QTabWidget::tabCloseRequested,
            this, &StudioMainWindow::onTabCloseRequested);
    connect(central_, &QTabWidget::currentChanged,
            this, &StudioMainWindow::refreshEditActions);

    // Previous/next tab buttons in the corner + Ctrl+Tab shortcuts.
    auto* pager = new QWidget();
    auto* pagerLayout = new QHBoxLayout(pager);
    pagerLayout->setContentsMargins(0, 0, 4, 0);
    pagerLayout->setSpacing(2);
    auto* prevTab = new QPushButton(tr("<"));
    prevTab->setFixedSize(24, 24);
    prevTab->setToolTip(tr("Previous tab (Ctrl+Shift+Tab)"));
    auto* nextTab = new QPushButton(tr(">"));
    nextTab->setFixedSize(24, 24);
    nextTab->setToolTip(tr("Next tab (Ctrl+Tab)"));
    pagerLayout->addWidget(prevTab);
    pagerLayout->addWidget(nextTab);
    central_->setCornerWidget(pager, Qt::TopRightCorner);
    connect(prevTab, &QPushButton::clicked, this, [this] { cycleTab(-1); });
    connect(nextTab, &QPushButton::clicked, this, [this] { cycleTab(1); });
    auto* nextShortcut = new QShortcut(QKeySequence::NextChild, this);
    connect(nextShortcut, &QShortcut::activated, this, [this] { cycleTab(1); });
    auto* prevShortcut = new QShortcut(QKeySequence::PreviousChild, this);
    connect(prevShortcut, &QShortcut::activated, this, [this] { cycleTab(-1); });

    buildCreditsTab();
    buildSettingsTab();

    // Scene editor tab: live viewport + hierarchy + inspector + source.
    sceneEditor_ = new SceneEditorTab(projectRoot_, session_);
    central_->addTab(sceneEditor_, tr("Scene"));
    sceneTab_ = sceneEditor_;
    connect(sceneEditor_, &SceneEditorTab::dirtyChanged, this,
            &StudioMainWindow::onSceneDirtyChanged);

    buildViewportTab();

    // Credits / Project Settings / Scene / Viewport stay pinned and
    // unclosable; file pages insert before them.
    for (int i = 0; i < central_->count(); ++i) {
        central_->widget(i)->setProperty("pageKind", "pinned");
    }
    refreshPinnedCloseButtons();
}

void StudioMainWindow::buildViewportTab() {
    previewTab_ = new PreviewViewport(projectRoot_, session_);
    central_->addTab(previewTab_, tr("Preview"));
}

void StudioMainWindow::onSceneDirtyChanged(bool dirty) {
    if (!sceneTab_) {
        return;
    }
    central_->setTabText(central_->indexOf(sceneTab_),
                         dirty ? sceneTabBaseTitle_ + " *" : sceneTabBaseTitle_);
}

void StudioMainWindow::buildCreditsTab() {
    QWidget* tab = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(tab);
    QVBoxLayout* left = new QVBoxLayout();
    creditsList_ = new QListWidget();
    left->addWidget(new QLabel(tr("Project credits blocks (in document order)")));
    left->addWidget(creditsList_);
    QHBoxLayout* form = new QHBoxLayout();
    personName_ = new QLineEdit();
    personName_->setPlaceholderText(tr("Name"));
    personRole_ = new QLineEdit();
    personRole_->setPlaceholderText(tr("Role"));
    form->addWidget(personName_);
    form->addWidget(personRole_);
    left->addLayout(form);
    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* add = new QPushButton(tr("Add person"));
    QPushButton* remove = new QPushButton(tr("Remove selected"));
    QPushButton* save = new QPushButton(tr("Save credits.xml"));
    btns->addWidget(add);
    btns->addWidget(remove);
    btns->addWidget(save);
    left->addLayout(btns);
    connect(add, &QPushButton::clicked, this, &StudioMainWindow::onAddPerson);
    connect(remove, &QPushButton::clicked, this, &StudioMainWindow::onRemovePerson);
    connect(save, &QPushButton::clicked, this, &StudioMainWindow::onSaveCredits);
    connect(creditsList_, &QListWidget::itemClicked, this, &StudioMainWindow::onCreditsSelected);
    layout->addLayout(left, 1);
    QVBoxLayout* right = new QVBoxLayout();
    right->addWidget(new QLabel(tr("Raw XML (read-only preview; Save writes the form model)")));
    creditsXmlView_ = new QPlainTextEdit();
    creditsXmlView_->setReadOnly(true);
    right->addWidget(creditsXmlView_);
    layout->addLayout(right, 1);
    central_->addTab(tab, tr("Credits"));
}

void StudioMainWindow::buildSettingsTab() {
    QWidget* tab = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(tab);
    QVBoxLayout* left = new QVBoxLayout();
    QFormLayout* form = new QFormLayout();
    fName_ = new QLineEdit();
    fWindow_ = new QLineEdit();
    fId_ = new QLineEdit();
    fVersion_ = new QLineEdit();
    fDesc_ = new QLineEdit();
    fEngine_ = new QLineEdit();
    fEngine_->setReadOnly(true); // mirrored from version.h; do not hand-edit
    fAuthor_ = new QLineEdit();
    fCopyright_ = new QLineEdit();
    form->addRow(tr("Name (single token)"), fName_);
    form->addRow(tr("Window name"), fWindow_);
    form->addRow(tr("Identifier"), fId_);
    form->addRow(tr("Version"), fVersion_);
    form->addRow(tr("Description"), fDesc_);
    form->addRow(tr("Incogine version (read-only)"), fEngine_);
    form->addRow(tr("Author"), fAuthor_);
    form->addRow(tr("Copyright"), fCopyright_);
    left->addLayout(form);
    left->addWidget(new QLabel(tr("<settings> inner XML (preserved verbatim; extensible)")));
    fSettingsRaw_ = new QPlainTextEdit();
    left->addWidget(fSettingsRaw_);
    QPushButton* save = new QPushButton(tr("Save project.xml"));
    connect(save, &QPushButton::clicked, this, &StudioMainWindow::onSaveProjectSettings);
    left->addWidget(save);
    layout->addLayout(left, 1);
    QVBoxLayout* right = new QVBoxLayout();
    right->addWidget(new QLabel(tr("Raw XML preview")));
    settingsXmlView_ = new QPlainTextEdit();
    settingsXmlView_->setReadOnly(true);
    right->addWidget(settingsXmlView_);
    layout->addLayout(right, 1);
    central_->addTab(tab, tr("Project Settings"));
}

void StudioMainWindow::buildDocks() {
    icg::studio::ProjectPaths paths(QString::fromStdString(projectRoot_).toStdString());
    // Fallback: ProjectPaths expects the repo root; our root IS the repo root.
    const QString srcRoot = QString::fromStdString(projectRoot_ + "/src");

    QDockWidget* projDock = new QDockWidget(tr("Project — src/"), this);
    projectModel_ = new QFileSystemModel(this);
    projectModel_->setRootPath(srcRoot);
    projectView_ = new QTreeView();
    projectView_->setModel(projectModel_);
    projectView_->setRootIndex(projectModel_->index(srcRoot));
    projectView_->setHeaderHidden(true);
    projectView_->setContextMenuPolicy(Qt::CustomContextMenu);
    projectView_->setDragEnabled(true);
    projectView_->setAcceptDrops(true);
    projectView_->setDropIndicatorShown(true);
    projectView_->setDragDropMode(QAbstractItemView::InternalMove);
    for (int c = 1; c < projectModel_->columnCount(); ++c) {
        projectView_->hideColumn(c);
    }
    projDock->setWidget(projectView_);
    addDockWidget(Qt::LeftDockWidgetArea, projDock);
    connect(projectView_, &QTreeView::doubleClicked, this, [this](const QModelIndex& idx) {
        auto* m = static_cast<QFileSystemModel*>(const_cast<QAbstractItemModel*>(idx.model()));
        const QString path = m->filePath(idx);
        if (QFileInfo(path).isFile()) {
            onOpenFile(path);
        }
    });
    connect(projectView_, &QTreeView::customContextMenuRequested,
            this, &StudioMainWindow::onTreeContextMenu);
    showLoadingOverlay(projectView_, projectModel_, srcRoot);

    QDockWidget* sceneDock = new QDockWidget(tr("Scenes (read-only)"), this);
    sceneTree_ = new QTreeWidget();
    sceneTree_->setHeaderLabels({tr("Scene"), tr("Detail")});
    sceneDock->setWidget(sceneTree_);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);
    connect(sceneTree_, &QTreeWidget::itemClicked, this, &StudioMainWindow::onSceneSelected);

    QDockWidget* assetsDock = new QDockWidget(tr("Asset Browser — src/assets/"), this);
    assetsModel_ = new QFileSystemModel(this);
    const QString assetsRoot = QString::fromStdString(projectRoot_ + "/src/assets");
    assetsModel_->setRootPath(assetsRoot);
    assetsView_ = new QTreeView();
    assetsView_->setModel(assetsModel_);
    assetsView_->setRootIndex(assetsModel_->index(assetsRoot));
    assetsView_->setHeaderHidden(true);
    assetsView_->setContextMenuPolicy(Qt::CustomContextMenu);
    assetsView_->setDragEnabled(true);
    assetsView_->setAcceptDrops(true);
    assetsView_->setDropIndicatorShown(true);
    assetsView_->setDragDropMode(QAbstractItemView::InternalMove);
    for (int c = 1; c < assetsModel_->columnCount(); ++c) {
        assetsView_->hideColumn(c);
    }
    assetsDock->setWidget(assetsView_);
    addDockWidget(Qt::LeftDockWidgetArea, assetsDock);
    connect(assetsView_, &QTreeView::doubleClicked, this, [this](const QModelIndex& idx) {
        auto* m = static_cast<QFileSystemModel*>(const_cast<QAbstractItemModel*>(idx.model()));
        const QString path = m->filePath(idx);
        if (QFileInfo(path).isFile()) {
            onOpenFile(path);
        }
    });
    connect(assetsView_, &QTreeView::customContextMenuRequested,
            this, &StudioMainWindow::onTreeContextMenu);
    showLoadingOverlay(assetsView_, assetsModel_, assetsRoot);

    QDockWidget* inspDock = new QDockWidget(tr("Inspector"), this);
    QLabel* insp = new QLabel(tr("Object model: Position / Scale / Rotation / Color\n"
                                 "Components: Transform, Sprite, ScriptComponent\n\n"
                                 "Live-object binding and property editing land after\n"
                                 "the scene format work (see docs/studio.md)."));
    insp->setWordWrap(true);
    insp->setMargin(8);
    inspDock->setWidget(insp);
    addDockWidget(Qt::RightDockWidgetArea, inspDock);

    QDockWidget* outDock = new QDockWidget(tr("Output / Build"), this);
    output_ = new QPlainTextEdit();
    output_->setReadOnly(true);
    outDock->setWidget(output_);
    addDockWidget(Qt::BottomDockWidgetArea, outDock);

    searchDock_ = new QDockWidget(tr("Search in files"), this);
    searchPanel_ = new SearchPanel(projectRoot_);
    searchDock_->setWidget(searchPanel_);
    addDockWidget(Qt::BottomDockWidgetArea, searchDock_);
    tabifyDockWidget(outDock, searchDock_);
    connect(searchPanel_, &SearchPanel::openFile, this,
            [this](const QString& path, int line) { onOpenFile(path, line); });

    auto* consoleDock = new QDockWidget(tr("Preview Console"), this);
    previewConsole_ = new QPlainTextEdit();
    previewConsole_->setReadOnly(true);
    previewConsole_->setMaximumBlockCount(5000);
    previewConsole_->setPlaceholderText(
        tr("Incogine process output appears here while a preview runs."));
    consoleDock->setWidget(previewConsole_);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
    tabifyDockWidget(outDock, consoleDock);
    connect(session_, &PreviewSession::consoleOutput, this,
            &StudioMainWindow::onPreviewConsole);
}

void StudioMainWindow::onPreviewConsole(const QString& text) {
    if (!previewConsole_) {
        return;
    }
    previewConsole_->moveCursor(QTextCursor::End);
    previewConsole_->insertPlainText(text);
    previewConsole_->moveCursor(QTextCursor::End);
}

namespace {

QString canonicalPath(const QString& path) {
    const QString canon = QFileInfo(path).canonicalFilePath();
    return canon.isEmpty() ? QFileInfo(path).absoluteFilePath() : canon;
}

bool isAudioSuffix(const QString& suffix) {
    return suffix == "ogg" || suffix == "wav" || suffix == "mp3" ||
           suffix == "flac" || suffix == "opus" || suffix == "mid" ||
           suffix == "midi" || suffix == "aiff" || suffix == "aif" ||
           suffix == "mod" || suffix == "xm" || suffix == "it" ||
           suffix == "s3m";
}

bool isFontSuffix(const QString& suffix) {
    return suffix == "ttf" || suffix == "otf" || suffix == "ttc" ||
           suffix == "woff" || suffix == "woff2";
}

} // namespace

bool StudioMainWindow::isPinnedTab(QWidget* page) const {
    return page && page->property("pageKind").toString() == "pinned";
}

int StudioMainWindow::firstPinnedTab() const {
    for (int i = 0; i < central_->count(); ++i) {
        if (isPinnedTab(central_->widget(i))) {
            return i;
        }
    }
    return central_->count();
}

QString StudioMainWindow::currentTabTitle() const {
    return central_->tabText(central_->currentIndex());
}

QString StudioMainWindow::tabTitleFor(const QString& path) const {    QString rel = QDir(QString::fromStdString(projectRoot_)).relativeFilePath(path);
    if (rel.length() > 120) {
        rel = "..." + rel.right(117);
    }
    return rel;
}

QString StudioMainWindow::baseTitleFor(const QString& kind, const QString& path) const {
    if (kind == "audio") {
        return tr("Audio - %1").arg(QFileInfo(path).fileName());
    }
    if (kind == "font") {
        return tr("Font - %1").arg(QFileInfo(path).fileName());
    }
    return tabTitleFor(path);
}

void StudioMainWindow::refreshPinnedCloseButtons() {
    auto* bar = central_->tabBar();
    for (int i = 0; i < central_->count(); ++i) {
        if (isPinnedTab(central_->widget(i))) {
            bar->setTabButton(i, QTabBar::RightSide, nullptr);
        }
    }
}

void StudioMainWindow::updatePageTitle(QWidget* page) {
    const int index = central_->indexOf(page);
    if (index < 0) {
        return;
    }
    QString title = page->property("baseTitle").toString();
    if (auto* editor = qobject_cast<CodeEditor*>(page)) {
        if (editor->editor()->document()->isModified()) {
            title = "* " + title;
        }
    }
    central_->setTabText(index, title);
}

CodeEditor* StudioMainWindow::currentCodeEditor() const {
    return qobject_cast<CodeEditor*>(central_->currentWidget());
}

void StudioMainWindow::refreshEditActions() {
    // Each code page owns its QTextDocument, so Undo/Redo histories are
    // naturally per-tab; the actions just target whichever page is front.
    auto* editor = currentCodeEditor();
    QPlainTextEdit* text = editor ? editor->editor() : nullptr;
    if (undoAction_) {
        undoAction_->setEnabled(text && text->document()->isUndoAvailable());
    }
    if (redoAction_) {
        redoAction_->setEnabled(text && text->document()->isRedoAvailable());
    }
}

QString StudioMainWindow::gameTargetName() const {
    // Desktop game target == the CMake top-level project name. Read it from
    // the configured build tree so a renamed project keeps working; the
    // Studio target itself is never built from here.
    QFile cache(QString::fromStdString(projectRoot_) + QStringLiteral("/build/CMakeCache.txt"));
    if (cache.open(QIODevice::ReadOnly)) {
        while (!cache.atEnd()) {
            const QString line = QString::fromUtf8(cache.readLine());
            if (line.startsWith(QStringLiteral("CMAKE_PROJECT_NAME:STATIC="))) {
                const QString name =
                    line.mid(int(QStringLiteral("CMAKE_PROJECT_NAME:STATIC=").size()))
                        .trimmed();
                if (!name.isEmpty()) {
                    return name;
                }
            }
        }
    }
    return QStringLiteral("Incogine");
}

void StudioMainWindow::startGameBuild() {
    const QString buildPath = QString::fromStdString(projectRoot_) + QStringLiteral("/build");
    logBuildLine(tr("Building target %1 (Debug) ...").arg(gameTargetName()));
    compileProcess_->setProgram(QStringLiteral("cmake"));
    compileProcess_->setArguments({QStringLiteral("--build"), buildPath,
                                   QStringLiteral("--target"), gameTargetName(),
                                   QStringLiteral("--config"), QStringLiteral("Debug")});
}

void StudioMainWindow::onCompileGame() {
    if (compileProcess_ && compileProcess_->state() != QProcess::NotRunning) {
        return; // already running; button is disabled anyway
    }
    if (!compileProcess_) {
        compileProcess_ = new QProcess(this);
        connect(compileProcess_, &QProcess::readyReadStandardOutput, this,
                &StudioMainWindow::onCompileOutput);
        connect(compileProcess_, &QProcess::readyReadStandardError, this,
                &StudioMainWindow::onCompileOutput);
        connect(compileProcess_, &QProcess::errorOccurred, this,
                &StudioMainWindow::onCompileError);
        connect(compileProcess_,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                &StudioMainWindow::onCompileFinished);
    }
    compileBuffer_.clear();
    compileAction_->setEnabled(false);
    const QString root = QString::fromStdString(projectRoot_);
    const QString buildPath = root + QStringLiteral("/build");
    if (!QFile::exists(buildPath + QStringLiteral("/CMakeCache.txt"))) {
        // First run: configure with the default generator (newest VS on
        // Windows), then chain into the build when it succeeds.
        compileConfiguring_ = true;
        logBuildLine(tr("Configuring %1 ...").arg(buildPath));
        compileProcess_->setProgram(QStringLiteral("cmake"));
        compileProcess_->setArguments(
            {QStringLiteral("-S"), root, QStringLiteral("-B"), buildPath});
    } else {
        compileConfiguring_ = false;
        startGameBuild();
    }
    compileProcess_->start();
}

void StudioMainWindow::onCompileOutput() {
    if (!compileProcess_) {
        return;
    }
    compileBuffer_ += QString::fromLocal8Bit(compileProcess_->readAllStandardOutput());
    compileBuffer_ += QString::fromLocal8Bit(compileProcess_->readAllStandardError());
    int nl = -1;
    while ((nl = compileBuffer_.indexOf(QLatin1Char('\n'))) >= 0) {
        QString line = compileBuffer_.left(nl);
        compileBuffer_ = compileBuffer_.mid(nl + 1);
        if (!line.isEmpty() && line.back() == QLatin1Char('\r')) {
            line.chop(1);
        }
        logBuildLine(line);
    }
}

void StudioMainWindow::onCompileError(QProcess::ProcessError error) {
    if (compileProcess_ && compileProcess_->state() == QProcess::NotRunning) {
        logBuildLine(tr("Could not start cmake (error %1). Is CMake on PATH?")
                         .arg(static_cast<int>(error)));
        compileConfiguring_ = false;
        compileAction_->setEnabled(true);
    }
}

void StudioMainWindow::onCompileFinished(int exitCode, QProcess::ExitStatus status) {
    if (!compileBuffer_.isEmpty()) {
        logBuildLine(compileBuffer_);
        compileBuffer_.clear();
    }
    const bool ok = (status == QProcess::NormalExit && exitCode == 0);
    if (compileConfiguring_) {
        compileConfiguring_ = false;
        if (ok) {
            startGameBuild(); // chain configure -> build, stay disabled
            compileProcess_->start();
            return;
        }
        logBuildLine(tr("Configure failed (exit %1).").arg(exitCode));
    } else if (ok) {
        logBuildLine(tr("Build finished: %1 OK.").arg(gameTargetName()));
    } else {
        logBuildLine(tr("Build finished with errors (exit %1).").arg(exitCode));
    }
    compileAction_->setEnabled(true);
}

CodeEditor* StudioMainWindow::openCodePage(const QString& path, int line) {
    const QString key = canonicalPath(path);
    if (QWidget* existing = filePages_.value(key, nullptr)) {
        central_->setCurrentWidget(existing);
        if (auto* editor = qobject_cast<CodeEditor*>(existing)) {
            editor->gotoLine(line);
            return editor;
        }
        return nullptr;
    }
    QFile f(path);
    // Raw bytes (no QIODevice::Text): Text mode would translate line
    // endings on read, and translating back on save doubles CRLF files
    // into blank lines. EOL/BOM ride along as page properties instead so
    // untouched files round-trip byte-identically.
    if (!f.open(QIODevice::ReadOnly)) {
        log(tr("Cannot open %1").arg(path));
        return nullptr;
    }
    const QByteArray raw = f.readAll();
    f.close();
    QByteArray body = raw;
    const bool hasBom = body.startsWith("\xEF\xBB\xBF");
    if (hasBom) {
        body = body.mid(3);
    }
    const bool isCrlf = body.contains("\r\n");
    QTextStream in(&body, QIODevice::ReadOnly);
    auto* editor = new CodeEditor();
    editor->setText(in.readAll());
    editor->setProperty("fileEol", isCrlf ? QStringLiteral("\r\n") : QStringLiteral("\n"));
    editor->setProperty("fileBom", hasBom);
    editor->setLanguageFromPath(path);
    editor->gotoLine(line);
    editor->editor()->document()->setModified(false);
    editor->setProperty("pageKind", "code");
    editor->setProperty("filePath", key);
    editor->setProperty("baseTitle", baseTitleFor("code", key));
    central_->insertTab(firstPinnedTab(), editor, editor->property("baseTitle").toString());
    filePages_.insert(key, editor);
    connect(editor->editor()->document(), &QTextDocument::modificationChanged,
            this, [this, editor](bool) { updatePageTitle(editor); });
    // Keep the Edit-menu Undo/Redo states in sync with this page.
    connect(editor->editor()->document(), &QTextDocument::undoAvailable,
            this, &StudioMainWindow::refreshEditActions);
    connect(editor->editor()->document(), &QTextDocument::redoAvailable,
            this, &StudioMainWindow::refreshEditActions);
    central_->setCurrentWidget(editor);
    refreshPinnedCloseButtons();
    log(tr("Opened %1").arg(path));
    return editor;
}

QWidget* StudioMainWindow::openAudioPage(const QString& path) {
    const QString key = canonicalPath(path);
    if (QWidget* existing = filePages_.value(key, nullptr)) {
        central_->setCurrentWidget(existing);
        return existing;
    }
#ifdef ICG_STUDIO_HAS_MULTIMEDIA
    auto* page = new AudioPreview(path);
    page->setProperty("pageKind", "audio");
    page->setProperty("filePath", key);
    page->setProperty("baseTitle", baseTitleFor("audio", path));
    central_->insertTab(firstPinnedTab(), page, page->property("baseTitle").toString());
    filePages_.insert(key, page);
    central_->setCurrentWidget(page);
    refreshPinnedCloseButtons();
    log(tr("Opened %1").arg(path));
    return page;
#else
    log(tr("Audio preview needs Qt Multimedia; opening as text."));
    return openCodePage(path, -1);
#endif
}

QWidget* StudioMainWindow::openFontPage(const QString& path) {
    const QString key = canonicalPath(path);
    if (QWidget* existing = filePages_.value(key, nullptr)) {
        central_->setCurrentWidget(existing);
        return existing;
    }
    auto* page = new FontPreview(path);
    page->setProperty("pageKind", "font");
    page->setProperty("filePath", key);
    page->setProperty("baseTitle", baseTitleFor("font", path));
    central_->insertTab(firstPinnedTab(), page, page->property("baseTitle").toString());
    filePages_.insert(key, page);
    central_->setCurrentWidget(page);
    refreshPinnedCloseButtons();
    log(tr("Opened %1").arg(path));
    return page;
}

bool StudioMainWindow::saveCodePage(CodeEditor* editor, const QString& path) {
    if (!editor) {
        return false;
    }
    QFile f(path);
    // Verbatim write: the document normalizes to \n internally, so restore
    // the file's original EOL/BOM instead of letting Text mode guess.
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        log(tr("Cannot write %1").arg(path));
        return false;
    }
    QString text = editor->text();
    if (editor->property("fileEol").toString() == QStringLiteral("\r\n")) {
        text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
    }
    QByteArray raw = text.toUtf8();
    if (editor->property("fileBom").toBool()) {
        raw.prepend("\xEF\xBB\xBF");
    }
    f.write(raw);
    f.close();
    editor->editor()->document()->setModified(false);
    updatePageTitle(editor);
    log(tr("Saved %1").arg(path));
    return true;
}

bool StudioMainWindow::closePage(QWidget* page) {
    if (!page || isPinnedTab(page)) {
        return true;
    }
    if (auto* editor = qobject_cast<CodeEditor*>(page)) {
        if (editor->editor()->document()->isModified()) {
            const QString path = page->property("filePath").toString();
            auto answer = QMessageBox::question(
                this, tr("Unsaved changes"),
                tr("%1 has unsaved changes. Save before closing?")
                    .arg(tabTitleFor(path)),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (answer == QMessageBox::Cancel) {
                return false;
            }
            if (answer == QMessageBox::Save && !saveCodePage(editor, path)) {
                return false;
            }
        }
    }
    filePages_.remove(page->property("filePath").toString());
    central_->removeTab(central_->indexOf(page));
    page->deleteLater();
    refreshPinnedCloseButtons();
    return true;
}

void StudioMainWindow::onTabCloseRequested(int index) {
    closePage(central_->widget(index));
}

void StudioMainWindow::cycleTab(int direction) {
    const int count = central_->count();
    if (count < 2) {
        return;
    }
    central_->setCurrentIndex((central_->currentIndex() + direction + count) % count);
}

void StudioMainWindow::showLoadingOverlay(QTreeView* view, QFileSystemModel* model,
                                          const QString& root) {
    if (!view || !model) {
        return;
    }
    if (auto* old = view->viewport()->findChild<QLabel*>("loadingOverlay")) {
        old->deleteLater();
    }
    auto* overlay = new QLabel(tr("Loading..."), view->viewport());
    overlay->setObjectName("loadingOverlay");
    overlay->setAlignment(Qt::AlignCenter);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setStyleSheet(
        "background-color: palette(base); padding: 8px; "
        "border: 1px solid palette(mid);");
    QWidget* viewport = view->viewport();
    overlay->adjustSize();
    overlay->move((viewport->width() - overlay->width()) / 2,
                  (viewport->height() - overlay->height()) / 2);
    overlay->show();
    // Hide once the requested root finishes. The emitted path string is
    // compared normalized (native separators/case differ per platform), and
    // as a backstop any listed rows also dismiss it — visible files mean
    // loading produced content, so the badge has served its purpose.
    connect(model, &QFileSystemModel::directoryLoaded, overlay,
            [overlay, root](const QString& path) {
                if (QDir::cleanPath(path).compare(QDir::cleanPath(root),
                                                  Qt::CaseInsensitive) == 0) {
                    overlay->hide();
                }
            });
    connect(model, &QFileSystemModel::rowsInserted, overlay,
            [overlay](const QModelIndex&, int, int) { overlay->hide(); });
}

void StudioMainWindow::onOpenFile(const QString& path, int line) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (isAudioSuffix(suffix)) {
        openAudioPage(path);
        return;
    }
    if (isFontSuffix(suffix)) {
        openFontPage(path);
        return;
    }
    if (!openCodePage(path, line)) {
        return;
    }

    // Route structured files to their form tabs as well.
    if (path.endsWith("/credits.xml") || path.endsWith("\\credits.xml")) {
        std::string error;
        if (icg::studio::CreditsXml::ParseFile(path.toStdString(), *credits_, error)) {
            creditsList_->clear();
            for (const auto& b : credits_->projectBlocks) {
                QString label;
                switch (b.type) {
                    case icg::studio::CreditBlock::Type::Header:
                        label = QString("header: %1").arg(QString::fromStdString(b.text));
                        break;
                    case icg::studio::CreditBlock::Type::Person:
                        label = QString("person: %1 (%2)").arg(QString::fromStdString(b.person.name),
                                                               QString::fromStdString(b.person.role));
                        break;
                    case icg::studio::CreditBlock::Type::Subheader:
                        label = QString("subheader: %1").arg(QString::fromStdString(b.text));
                        break;
                    case icg::studio::CreditBlock::Type::Grid:
                        label = QString("grid: %1 entries").arg(b.names.size());
                        break;
                    default:
                        label = QString("unknown block (preserved)");
                        break;
                }
                creditsList_->addItem(label);
            }
            creditsXmlView_->setPlainText(QString::fromStdString(credits_->Serialize()));
        } else {
            log(tr("credits.xml parse: %1").arg(QString::fromStdString(error)));
        }
    }
    if (path.endsWith("/project.xml") || path.endsWith("\\project.xml")) {
        std::string error;
        if (icg::studio::ProjectXml::ParseFile(path.toStdString(), *project_, error)) {
            fName_->setText(QString::fromStdString(project_->name));
            fWindow_->setText(QString::fromStdString(project_->windowName));
            fId_->setText(QString::fromStdString(project_->identifier));
            fVersion_->setText(QString::fromStdString(project_->version));
            fDesc_->setText(QString::fromStdString(project_->description));
            fEngine_->setText(QString::fromStdString(project_->incogineVersion));
            fAuthor_->setText(QString::fromStdString(project_->author));
            fCopyright_->setText(QString::fromStdString(project_->copyright));
            fSettingsRaw_->setPlainText(QString::fromStdString(project_->settingsInner));
            settingsXmlView_->setPlainText(QString::fromStdString(project_->Serialize()));
        } else {
            log(tr("project.xml parse: %1").arg(QString::fromStdString(error)));
        }
    }
}

void StudioMainWindow::onSaveCurrentFile() {
    auto* editor = currentCodeEditor();
    if (!editor) {
        log(tr("Nothing to save."));
        return;
    }
    saveCodePage(editor, editor->property("filePath").toString());
}

void StudioMainWindow::onSaveAllFiles() {
    int saved = 0;
    for (QWidget* page : filePages_) {
        if (auto* editor = qobject_cast<CodeEditor*>(page)) {
            if (editor->editor()->document()->isModified() &&
                saveCodePage(editor, page->property("filePath").toString())) {
                ++saved;
            }
        }
    }
    log(tr("Saved %1 file(s).").arg(saved));
}

void StudioMainWindow::onTreeContextMenu(const QPoint& pos) {
    auto* view = qobject_cast<QTreeView*>(sender());
    auto* model = qobject_cast<QFileSystemModel*>(view ? view->model() : nullptr);
    if (!view || !model) {
        return;
    }
    contextIndex_ = view->indexAt(pos);
    contextModel_ = model;
    if (!contextIndex_.isValid()) {
        contextIndex_ = view->rootIndex();
    }

    QMenu menu(this);
    menu.addAction(tr("New file..."), this, &StudioMainWindow::onNewFile);
    menu.addAction(tr("New folder..."), this, &StudioMainWindow::onNewFolder);
    if (model->isDir(contextIndex_)) {
        menu.addSeparator();
        menu.addAction(tr("Refresh"), this, &StudioMainWindow::onRefreshProject);
    } else if (contextIndex_.isValid()) {
        menu.addAction(tr("Rename..."), this, &StudioMainWindow::onRenameItem);
        menu.addAction(tr("Delete"), this, &StudioMainWindow::onDeleteItems);
    }
    menu.exec(view->viewport()->mapToGlobal(pos));
}

static QString contextDir(QFileSystemModel* model, const QModelIndex& index) {
    if (!index.isValid()) {
        return model->rootPath();
    }
    if (model->isDir(index)) {
        return model->filePath(index);
    }
    return QFileInfo(model->filePath(index)).absolutePath();
}

void StudioMainWindow::onNewFile() {
    if (!contextModel_) {
        return;
    }
    const QString dir = contextDir(contextModel_, contextIndex_);
    const QString name = QInputDialog::getText(this, tr("New file"), tr("File name:"));
    if (name.isEmpty()) {
        return;
    }
    const QString path = QDir(dir).filePath(name);
    if (QFile::exists(path)) {
        log(tr("Already exists: %1").arg(path));
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        log(tr("Cannot create %1").arg(path));
        return;
    }
    log(tr("Created %1").arg(path));
    onOpenFile(path);
}

void StudioMainWindow::onNewFolder() {
    if (!contextModel_) {
        return;
    }
    const QString dir = contextDir(contextModel_, contextIndex_);
    // Script folders included: Studio never reserves language names —
    // any folder (e.g. src/scripts/newfolder/) is allowed as-is.
    const QString name = QInputDialog::getText(this, tr("New folder"), tr("Folder name:"));
    if (name.isEmpty()) {
        return;
    }
    const QString path = QDir(dir).filePath(name);
    if (!QDir().mkpath(path)) {
        log(tr("Cannot create %1").arg(path));
        return;
    }
    log(tr("Created folder %1").arg(path));
}

void StudioMainWindow::onRenameItem() {
    if (!contextModel_ || !contextIndex_.isValid() || contextModel_->isDir(contextIndex_)) {
        return;
    }
    const QString oldPath = contextModel_->filePath(contextIndex_);
    const QFileInfo info(oldPath);
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name:"),
                                               QLineEdit::Normal, info.fileName());
    if (name.isEmpty() || name == info.fileName()) {
        return;
    }
    const QString newPath = QDir(info.absolutePath()).filePath(name);
    if (QFile::exists(newPath) ||
        !QFile::rename(oldPath, newPath)) {
        log(tr("Cannot rename to %1").arg(newPath));
        return;
    }
    log(tr("Renamed %1 -> %2 (update #includes / references by hand as needed).")
            .arg(oldPath, newPath));
    const QString oldKey = canonicalPath(oldPath);
    if (QWidget* page = filePages_.take(oldKey)) {
        const QString newKey = canonicalPath(newPath);
        page->setProperty("filePath", newKey);
        page->setProperty("baseTitle",
                          baseTitleFor(page->property("pageKind").toString(), newPath));
        filePages_.insert(newKey, page);
        updatePageTitle(page);
    }
}

void StudioMainWindow::onDeleteItems() {
    if (!contextModel_ || !contextIndex_.isValid()) {
        return;
    }
    const QString path = contextModel_->filePath(contextIndex_);
    const QFileInfo info(path);
    auto answer = QMessageBox::question(
        this, tr("Delete"),
        tr("Delete %1?\n\nRenaming/deleting source files may break #includes, "
           "scene references, and script attachments — update them by hand.")
            .arg(path));
    if (answer != QMessageBox::Yes) {
        return;
    }
    bool ok = info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
    if (!ok) {
        log(tr("Cannot delete %1").arg(path));
        return;
    }
    log(tr("Deleted %1").arg(path));
    if (QWidget* page = filePages_.take(canonicalPath(path))) {
        central_->removeTab(central_->indexOf(page));
        page->deleteLater();
        refreshPinnedCloseButtons();
    }
}

void StudioMainWindow::onFocusSearch() {
    if (searchDock_) {
        searchDock_->show();
        searchDock_->raise();
    }
}

void StudioMainWindow::onRefreshProject() {
    // Force the file models to rescan (they also watch the fs live).
    const QString srcRoot = QString::fromStdString(projectRoot_ + "/src");
    const QString assetsRoot = QString::fromStdString(projectRoot_ + "/src/assets");
    projectModel_->setRootPath(QString());
    projectModel_->setRootPath(srcRoot);
    projectView_->setRootIndex(projectModel_->index(srcRoot));
    assetsModel_->setRootPath(QString());
    assetsModel_->setRootPath(assetsRoot);
    assetsView_->setRootIndex(assetsModel_->index(assetsRoot));
    showLoadingOverlay(projectView_, projectModel_, srcRoot);
    showLoadingOverlay(assetsView_, assetsModel_, assetsRoot);
    onDiscoverScenes();
    // Preload the structured models so the form tabs are useful immediately.
    onOpenFile(QString::fromStdString(projectRoot_ + "/src/project.xml"));
    onOpenFile(QString::fromStdString(projectRoot_ + "/src/credits.xml"));
    // Leaves their code pages open; user picks from here.
}

void StudioMainWindow::onDiscoverScenes() {
    sceneTree_->clear();
    auto scenes = icg::studio::SceneDiscovery::Scan(projectRoot_ + "/src/scenes");
    for (const auto& s : scenes) {
        QString title = QString::fromStdString(s.className);
        if (!s.declaredName.empty()) {
            title += QString(" (\"%1\")").arg(QString::fromStdString(s.declaredName));
        }
        QTreeWidgetItem* top = new QTreeWidgetItem(sceneTree_, {title, s.inPurokoLib ? tr("Puroko lib") : tr("exe-only")});
        top->setData(0, Qt::UserRole, QString::fromStdString(s.headerFile));
        top->setData(1, Qt::UserRole, QString::fromStdString(s.sourceFile));
        top->setData(0, Qt::UserRole + 1, QString::fromStdString(s.className));
        top->setData(0, Qt::UserRole + 2, QString::fromStdString(s.declaredName));
        new QTreeWidgetItem(top, {tr("Header"), QString::fromStdString(s.headerFile)});
        if (!s.sourceFile.empty()) {
            new QTreeWidgetItem(top, {tr("Source"), QString::fromStdString(s.sourceFile)});
        }
    }
    log(tr("Discovered %1 scene(s) (read-only).").arg(scenes.size()));
}

void StudioMainWindow::onSceneSelected(QTreeWidgetItem* item) {
    if (!item || !sceneTab_) {
        return;
    }
    QTreeWidgetItem* top = item;
    while (top->parent()) {
        top = top->parent();
    }
    // Scene sidebar selections drive the Scene editor tab (no Code tab is
    // opened for scene files) and the Preview launch target.
    const QString header = top->data(0, Qt::UserRole).toString();
    if (!header.isEmpty()) {
        QString sceneName = top->data(0, Qt::UserRole + 2).toString();
        QString className = top->data(0, Qt::UserRole + 1).toString();
        if (sceneName.isEmpty()) {
            sceneName = className;
        }
        if (className == sceneEditor_->sceneClass() && !sceneEditor_->isDirty()) {
            central_->setCurrentWidget(sceneTab_); // same scene, nothing to do
            return;
        }
        // Unsaved scene edits never survive a switch (setScene re-parses
        // from disk), so offer Save / Don't Save / Cancel first.
        if (sceneEditor_->isDirty()) {
            auto answer = QMessageBox::question(
                this, tr("Unsaved scene changes"),
                tr("Scene %1 has unsaved changes. Save before switching?")
                    .arg(sceneEditor_->sceneClass()),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (answer == QMessageBox::Cancel) {
                return;
            }
            if (answer == QMessageBox::Save && !sceneEditor_->saveSceneToSource()) {
                return; // write failed (noted in the tab); stay put
            }
        }
        selectedSceneClass_ = className;
        sceneEditor_->setScene(className, header,
                               top->data(1, Qt::UserRole).toString());
        previewTab_->setLaunchScene(className);
        sceneTabBaseTitle_ = tr("Scene - %1").arg(sceneName);
        central_->setTabText(central_->indexOf(sceneTab_), sceneTabBaseTitle_);
        central_->setCurrentWidget(sceneTab_);
    }
}

void StudioMainWindow::onCreditsSelected() {
    // Selecting a block loads person fields when applicable.
    const int row = creditsList_->currentRow();
    if (row < 0 || row >= static_cast<int>(credits_->projectBlocks.size())) {
        return;
    }
    const auto& b = credits_->projectBlocks[row];
    if (b.type == icg::studio::CreditBlock::Type::Person) {
        personName_->setText(QString::fromStdString(b.person.name));
        personRole_->setText(QString::fromStdString(b.person.role));
    }
}

void StudioMainWindow::onSaveCredits() {
    const QString path = QString::fromStdString(projectRoot_ + "/src/credits.xml");
    std::string error;
    if (!credits_->SaveFile(path.toStdString(), error)) {
        log(tr("Save credits.xml failed: %1").arg(QString::fromStdString(error)));
        return;
    }
    creditsXmlView_->setPlainText(QString::fromStdString(credits_->Serialize()));
    log(tr("Saved %1").arg(path));
}

void StudioMainWindow::onAddPerson() {
    credits_->AddPerson(personName_->text().toStdString(), personRole_->text().toStdString());
    creditsList_->addItem(QString("person: %1 (%2)").arg(personName_->text(), personRole_->text()));
    log(tr("Added person (unsaved until Save)."));
}

void StudioMainWindow::onRemovePerson() {
    const int row = creditsList_->currentRow();
    if (row < 0) {
        return;
    }
    // Map list row -> person index among Person blocks.
    int personIdx = -1;
    int seen = 0;
    for (int i = 0; i <= row && i < static_cast<int>(credits_->projectBlocks.size()); ++i) {
        if (credits_->projectBlocks[i].type == icg::studio::CreditBlock::Type::Person) {
            if (i == row) {
                personIdx = seen;
            }
            ++seen;
        }
    }
    if (personIdx >= 0 && credits_->RemovePerson(personIdx)) {
        delete creditsList_->takeItem(row);
        log(tr("Removed person (unsaved until Save)."));
    } else {
        log(tr("Selected row is not a person entry."));
    }
}

void StudioMainWindow::onSaveProjectSettings() {
    project_->name = fName_->text().toStdString();
    project_->windowName = fWindow_->text().toStdString();
    project_->identifier = fId_->text().toStdString();
    project_->version = fVersion_->text().toStdString();
    project_->description = fDesc_->text().toStdString();
    project_->author = fAuthor_->text().toStdString();
    project_->copyright = fCopyright_->text().toStdString();
    project_->settingsInner = fSettingsRaw_->toPlainText().toStdString();
    // incogine_version stays mirrored from version.h; keep the loaded value.
    if (project_->name.find(' ') != std::string::npos) {
        log(tr("Warning: <name> must be a single token (executable filename)."));
    }
    std::string error;
    const QString path = QString::fromStdString(projectRoot_ + "/src/project.xml");
    if (!project_->SaveFile(path.toStdString(), error)) {
        log(tr("Save project.xml failed: %1").arg(QString::fromStdString(error)));
        return;
    }
    settingsXmlView_->setPlainText(QString::fromStdString(project_->Serialize()));
    log(tr("Saved %1 (CMake re-extracts identity on next configure).").arg(path));
}

void StudioMainWindow::onPackIncoba() {
    const std::string src = projectRoot_ + "/src/assets";
    const std::string outDir = projectRoot_ + "/incoba_staging";
    std::string error;
    if (!icg::studio::incoba::PackSplit(src, outDir, "a",
                                        icg::studio::incoba::kDefaultMaxBundleBytes, error)) {
        log(tr("Pack failed: %1").arg(QString::fromStdString(error)));
        return;
    }
    log(tr("Packed %1 -> %2/ (index.incobai + bundles; ship the folder as the release assets/).")
            .arg(QString::fromStdString(src), QString::fromStdString(outDir)));
}

void StudioMainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    QMainWindow::closeEvent(event);
}

bool StudioMainWindow::restoreGeometryFromSettings() {
    const QSettings settings;
    if (!settings.contains("geometry")) {
        return false;
    }
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    return true;
}

void StudioMainWindow::log(const QString& msg) {
    output_->appendPlainText(msg);
}

// Build-console line: errors red, warnings yellow, everything else in the
// theme's default text color (palette-driven, so dark/light mode both
// read correctly). Plain-text insert keeps the existing log content
// intact — no HTML round trip.
void StudioMainWindow::logBuildLine(const QString& line) {
    static const QRegularExpression isError(
        QStringLiteral("\\berror\\b|\\bfatal\\b|\\bfailed\\b|:error "),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression isWarning(
        QStringLiteral("\\bwarning\\b|:warning "),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression isZeroCount(
        QStringLiteral("^\\s*0\\s+(error|warning)"),
        QRegularExpression::CaseInsensitiveOption);
    QColor color = output_->palette().color(QPalette::Text);
    if (!isZeroCount.match(line).hasMatch()) {
        if (isError.match(line).hasMatch()) {
            color = Qt::red;
        } else if (isWarning.match(line).hasMatch()) {
            color = Qt::darkYellow; // readable on both dark and light themes
        }
    }
    QTextCursor cursor(output_->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(color);
    cursor.insertText(line + QStringLiteral("\n"), format);
    output_->ensureCursorVisible();
}
