// Incogine Studio — live preview viewport implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_viewport.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <cstring>

#include "../../core/preview/preview_protocol.h"
#include "../core/preview_exe.h"
#include "../core/project_xml.h"
#include "../core/scene_cpp.h"
#include "../core/scene_discovery.h"

// ---- PreviewCanvas ----

PreviewCanvas::PreviewCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumSize(480, 270);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: black;");
}

void PreviewCanvas::setFrame(const std::vector<uint8_t>& rgba, uint32_t width,
                             uint32_t height) {
    if (rgba.size() < static_cast<size_t>(width) * height * 4) {
        return;
    }
    frame_ = QImage(static_cast<int>(width), static_cast<int>(height),
                    QImage::Format_RGBA8888);
    std::memcpy(frame_.bits(), rgba.data(), static_cast<size_t>(width) * height * 4);
    update();
}

void PreviewCanvas::clear() {
    frame_ = QImage();
    update();
}

void PreviewCanvas::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (frame_.isNull()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, tr("No signal — launch the game preview"));
        return;
    }
    // Fit keeping aspect; flip vertically (frames arrive bottom-up).
    const QSize fitted = frame_.size().scaled(size(), Qt::KeepAspectRatio);
    const int x = (width() - fitted.width()) / 2;
    const int y = (height() - fitted.height()) / 2;
    painter.save();
    painter.translate(x, y + fitted.height());
    painter.scale(static_cast<qreal>(fitted.width()) / frame_.width(),
                  -static_cast<qreal>(fitted.height()) / frame_.height());
    painter.drawImage(QRect(0, 0, frame_.width(), frame_.height()), frame_);
    painter.restore();
}

// ---- PreviewViewport ----

namespace {

QString formatDouble(double v) {
    return QString::number(v, 'g', 6);
}

} // namespace

PreviewViewport::PreviewViewport(const std::string& projectRoot, QWidget* parent)
    : QWidget(parent), projectRoot_(projectRoot) {
    proc_ = new QProcess(this);

    canvas_ = new PreviewCanvas();

    exeField_ = new QLineEdit();
    exeField_->setPlaceholderText(tr("Game executable (e.g. build/Incogine.exe)..."));
    auto* browseButton = new QPushButton(tr("Browse..."));
    launchButton_ = new QPushButton(tr("Launch preview"));
    stopButton_ = new QPushButton(tr("Stop"));
    stopButton_->setEnabled(false);
    status_ = new QLabel(tr("Stopped."));

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Game:")));
    top->addWidget(exeField_, 1);
    top->addWidget(browseButton);
    top->addWidget(launchButton_);
    top->addWidget(stopButton_);

    sceneCombo_ = new QComboBox();
    QHBoxLayout* sceneRow = new QHBoxLayout();
    sceneRow->addWidget(new QLabel(tr("Scene:")));
    sceneRow->addWidget(sceneCombo_, 1);

    objectList_ = new QListWidget();
    objectList_->setMaximumHeight(110);

    auto* grid = new QGridLayout();
    const char* axes[3] = {"X", "Y", "Z"};
    QDoubleSpinBox** spins[3] = {posSpin_, rotSpin_, scaleSpin_};
    const char* rows[3] = {"Position", "Rotation", "Scale"};
    for (int r = 0; r < 3; ++r) {
        grid->addWidget(new QLabel(tr(rows[r])), r, 0);
        for (int c = 0; c < 3; ++c) {
            spins[r][c] = new QDoubleSpinBox();
            spins[r][c]->setRange(-100000.0, 100000.0);
            spins[r][c]->setDecimals(3);
            if (r == 2) {
                spins[r][c]->setValue(1.0);
            }
            grid->addWidget(new QLabel(tr(axes[c])), r, 1 + c * 2);
            grid->addWidget(spins[r][c], r, 2 + c * 2);
        }
    }

    applyButton_ = new QPushButton(tr("Apply live"));
    applyButton_->setToolTip(tr("Move the running object over the preview channel"));
    saveButton_ = new QPushButton(tr("Save to source"));
    saveButton_->setToolTip(tr("Rewrite the scene .cpp via the round-trip parser"));
    applyButton_->setEnabled(false);
    saveButton_->setEnabled(false);
    QHBoxLayout* editRow = new QHBoxLayout();
    editRow->addWidget(applyButton_);
    editRow->addWidget(saveButton_);
    editRow->addStretch(1);

    noteLabel_ = new QLabel();
    noteLabel_->setWordWrap(true);
    noteLabel_->setStyleSheet("color: palette(mid);");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(canvas_, 1);
    layout->addWidget(status_);
    layout->addLayout(sceneRow);
    layout->addWidget(objectList_);
    layout->addLayout(grid);
    layout->addLayout(editRow);
    layout->addWidget(noteLabel_);

    exeBaseName_ = exeBaseName();
    exeField_->setText(resolveExe());
    refreshScenes();

    poll_ = new QTimer(this);
    poll_->setInterval(33); // ~30 Hz display polling
    connect(poll_, &QTimer::timeout, this, &PreviewViewport::onPoll);

    connect(browseButton, &QPushButton::clicked, this, &PreviewViewport::onBrowse);
    connect(launchButton_, &QPushButton::clicked, this, &PreviewViewport::onLaunch);
    connect(stopButton_, &QPushButton::clicked, this, &PreviewViewport::onStop);
    connect(proc_, &QProcess::finished, this, &PreviewViewport::onProcessFinished);
    connect(sceneCombo_, &QComboBox::currentIndexChanged,
            this, &PreviewViewport::onSceneChanged);
    connect(objectList_, &QListWidget::itemClicked,
            this, &PreviewViewport::onObjectSelected);
    connect(applyButton_, &QPushButton::clicked, this, &PreviewViewport::onApplyLive);
    connect(saveButton_, &QPushButton::clicked, this, &PreviewViewport::onSaveToSource);
}

QString PreviewViewport::exeBaseName() {
    // Executable stem from the project identity (<name> in src/project.xml
    // becomes the binary name — single token, no spaces).
    icg::studio::ProjectXml project;
    std::string error;
    if (icg::studio::ProjectXml::ParseFile(
            projectRoot_ + "/src/project.xml", project, error) &&
        !project.name.empty()) {
        return QString::fromStdString(project.name);
    }
    return tr("Incogine");
}

QString PreviewViewport::resolveExe() {
    QSettings settings;
    const QString saved = settings.value("previewExe").toString();
    if (!saved.isEmpty() && QFileInfo::exists(saved)) {
        return saved; // explicit override (still binding-checked at launch)
    }
    // CMake-aware resolution: verified build trees first, Studio-sibling
    // dirs as fallback. The binding check at launch is the real gate.
    const QString studioDir = QCoreApplication::applicationDirPath();
    icg::studio::preview::PreviewExeInfo info;
    std::string error;
    if (icg::studio::preview::LocatePreviewExe(
            projectRoot_, exeBaseName_.toStdString(),
            {studioDir.toStdString(), (studioDir + "/..").toStdString(),
             (studioDir + "/../..").toStdString()},
            info, error)) {
        return QString::fromStdString(info.exePath);
    }
    setNote(tr("Game executable: %1").arg(QString::fromStdString(error)));
    return saved;
}

void PreviewViewport::setStatus(const QString& text) {
    status_->setText(text);
}

void PreviewViewport::setNote(const QString& text) {
    noteLabel_->setText(text);
}

void PreviewViewport::onBrowse() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select game executable"), exeField_->text());
    if (!path.isEmpty()) {
        exeField_->setText(path);
        QSettings settings;
        settings.setValue("previewExe", path);
    }
}

void PreviewViewport::onLaunch() {
    if (proc_->state() != QProcess::NotRunning) {
        return;
    }
    QString exe = exeField_->text();
    if (exe.isEmpty()) {
        exe = resolveExe();
        exeField_->setText(exe);
    }
    // Dev-build binding: refuse foreign executables, released binaries,
    // and swapped-in files with a visible error.
    icg::studio::preview::PreviewBinding binding;
    std::string verifyError;
    if (!icg::studio::preview::VerifyPreviewExe(projectRoot_,
                                                exeBaseName_.toStdString(),
                                                exe.toStdString(), binding,
                                                verifyError)) {
        QMessageBox::warning(this, tr("Preview refused"),
                             QString::fromStdString(verifyError));
        setStatus(tr("Launch refused: %1")
                      .arg(QString::fromStdString(verifyError)));
        return;
    }
    QSettings settings;
    settings.setValue("previewExe", QString::fromStdString(binding.exePath));
    exeField_->setText(QString::fromStdString(binding.exePath));
    QStringList args;
    const QString scene = sceneCombo_->currentData().toString();
    if (scene.isEmpty()) {
        args << "--studio-preview";
    } else {
        args << ("--studio-preview=" + scene);
    }
    sessionToken_ = 0;
    while (sessionToken_ == 0) {
        sessionToken_ = QRandomGenerator::global()->generate64();
    }
    args << ("--studio-token=" +
             QString::number(sessionToken_, 16));
    client_.Disconnect();
    connected_ = false;
    canvas_->clear();
    proc_->setWorkingDirectory(QFileInfo(exe).absolutePath());
    proc_->start(exe, args);
    if (!proc_->waitForStarted(5000)) {
        setStatus(tr("Failed to start the game process."));
        return;
    }
    launchButton_->setEnabled(false);
    stopButton_->setEnabled(true);
    poll_->start();
    setStatus(tr("Game started — waiting for frames..."));
}

void PreviewViewport::onStop() {
    if (proc_->state() == QProcess::NotRunning) {
        return;
    }
    std::string error;
    if (connected_ && !client_.SendCommand(ICG_PREVIEW_CMD_QUIT, 3000, error)) {
        setStatus(tr("Engine did not acknowledge quit — terminating."));
    }
    proc_->terminate();
    if (!proc_->waitForFinished(3000)) {
        proc_->kill();
    }
}

void PreviewViewport::onPoll() {
    if (proc_->state() == QProcess::NotRunning) {
        return; // onProcessFinished reports it
    }
    if (!connected_) {
        std::string error;
        if (client_.Connect("incogine_preview", sessionToken_, error)) {
            connected_ = true;
        } else if (!error.empty() && error.find("token mismatch") != std::string::npos) {
            QMessageBox::warning(this, tr("Session token mismatch"),
                                 tr("This is not the development build Studio launched."));
            setStatus(tr("Token mismatch — stopping foreign session."));
            onStop();
        }
        return;
    }
    uint32_t width = 0, height = 0, seq = 0;
    if (client_.TryFrame(frameBytes_, width, height, seq)) {
        canvas_->setFrame(frameBytes_, width, height);
        setStatus(tr("Live %1x%2 #%3").arg(width).arg(height).arg(seq));
    }
}

void PreviewViewport::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    poll_->stop();
    client_.Disconnect();
    connected_ = false;
    launchButton_->setEnabled(true);
    stopButton_->setEnabled(false);
    setStatus(status == QProcess::NormalExit
                  ? tr("Game exited (code %1).").arg(exitCode)
                  : tr("Game process crashed."));
}

void PreviewViewport::refreshScenes() {
    sceneCombo_->blockSignals(true);
    sceneCombo_->clear();
    scenePaths_.clear();
    sceneCombo_->addItem(tr("Default boot"), QString());
    const auto scenes = icg::studio::SceneDiscovery::Scan(projectRoot_ + "/src/scenes");
    for (const auto& scene : scenes) {
        const QString className = QString::fromStdString(scene.className);
        QString label = className;
        if (!scene.declaredName.empty()) {
            label += QString(" (\"%1\")").arg(QString::fromStdString(scene.declaredName));
        }
        QStringList paths;
        paths << QString::fromStdString(scene.headerFile)
              << QString::fromStdString(scene.sourceFile);
        sceneCombo_->addItem(label, className);
        scenePaths_.insert(className, paths);
    }
    sceneCombo_->blockSignals(false);
    refreshObjects();
}

bool PreviewViewport::selectPreviewScene(const QString& className) {
    const int index = sceneCombo_->findData(className);
    if (index < 0) {
        return false;
    }
    sceneCombo_->setCurrentIndex(index);
    return true;
}

QString PreviewViewport::selectedPreviewScene() const {
    return sceneCombo_ ? sceneCombo_->currentData().toString() : QString();
}

void PreviewViewport::onSceneChanged(int /*index*/) {
    refreshObjects();
}

void PreviewViewport::refreshObjects() {
    objectList_->clear();
    applyButton_->setEnabled(false);
    saveButton_->setEnabled(false);
    currentVar_.clear();
    currentHasId_ = false;
    currentSceneOk_ = false;

    const QStringList paths = scenePaths_.value(sceneCombo_->currentData().toString());
    if (paths.size() < 2 || paths[1].isEmpty()) {
        setNote(tr("Pick a scene to list its parser-known objects."));
        return;
    }
    currentHeader_ = paths[0];
    currentSource_ = paths[1];
    std::string error;
    if (!icg::studio::scenecpp::ParseSceneFiles(currentHeader_.toStdString(),
                                                currentSource_.toStdString(),
                                                currentScene_, error)) {
        setNote(tr("Scene source uses unrecognized patterns — text editing only."));
        return;
    }
    currentSceneOk_ = true;
    for (const auto& obj : currentScene_.model.objects) {
        QString label = QString::fromStdString(
            obj.displayName.empty() ? obj.varName : obj.displayName);
        label += QString(" (%1)").arg(QString::fromStdString(obj.varName));
        label += obj.hasId ? QString(" [#%1]").arg(obj.id)
                           : tr(" [no id]");
        objectList_->addItem(label);
        QListWidgetItem* item = objectList_->item(objectList_->count() - 1);
        item->setData(Qt::UserRole, QString::fromStdString(obj.varName));
    }
    if (currentScene_.model.objects.empty()) {
        setNote(tr("No parser-known objects (only Square/Cube/Object constructions "
                   "in the recognized patterns are editable)."));
    } else {
        setNote(tr("Select an object, then Apply live (running game) or Save to source."));
    }
}

void PreviewViewport::onObjectSelected() {
    QListWidgetItem* item = objectList_->currentItem();
    applyButton_->setEnabled(false);
    saveButton_->setEnabled(false);
    if (!item || !currentSceneOk_) {
        return;
    }
    currentVar_ = item->data(Qt::UserRole).toString();
    const icg::studio::scenecpp::ObjectModel* found = nullptr;
    for (const auto& obj : currentScene_.model.objects) {
        if (obj.varName == currentVar_.toStdString()) {
            found = &obj;
            break;
        }
    }
    if (!found) {
        return;
    }
    auto fill = [](QDoubleSpinBox* spins[3],
                   const icg::studio::scenecpp::VecExpr& vec, bool has, double fallback) {
        for (int i = 0; i < 3; ++i) {
            const double v =
                (has && vec.numeric && i < static_cast<int>(vec.values.size()))
                    ? vec.values[i]
                    : fallback;
            spins[i]->setValue(v);
        }
    };
    fill(posSpin_, found->position, found->hasPosition, 0.0);
    fill(rotSpin_, found->rotation, found->hasRotation, 0.0);
    fill(scaleSpin_, found->scale, found->hasScale, 1.0);
    currentId_ = found->id;
    currentHasId_ = found->hasId;
    saveButton_->setEnabled(true);
    if (currentHasId_) {
        applyButton_->setEnabled(connected_);
        setNote(tr("Live editing #%1. Save to source persists (assigns an id first "
                   "when missing).")
                    .arg(currentId_));
    } else {
        setNote(tr("No file id yet — Save to source assigns one first; "
                   "Apply live needs a relaunch afterwards."));
    }
}

void PreviewViewport::onApplyLive() {
    if (currentVar_.isEmpty() || !currentHasId_) {
        return;
    }
    if (!connected_) {
        setNote(tr("Not connected — launch the preview first."));
        return;
    }
    const float pos[3] = {static_cast<float>(posSpin_[0]->value()),
                          static_cast<float>(posSpin_[1]->value()),
                          static_cast<float>(posSpin_[2]->value())};
    const float rot[3] = {static_cast<float>(rotSpin_[0]->value()),
                          static_cast<float>(rotSpin_[1]->value()),
                          static_cast<float>(rotSpin_[2]->value())};
    const float scale[3] = {static_cast<float>(scaleSpin_[0]->value()),
                            static_cast<float>(scaleSpin_[1]->value()),
                            static_cast<float>(scaleSpin_[2]->value())};
    std::string error;
    if (!client_.SendTransform(currentId_, pos, rot, scale, 2000, error)) {
        setNote(tr("Live apply failed: %1").arg(QString::fromStdString(error)));
        return;
    }
    setNote(tr("Applied live to #%1 (not yet saved to source).").arg(currentId_));
}

void PreviewViewport::onSaveToSource() {
    if (currentVar_.isEmpty() || !currentSceneOk_) {
        return;
    }
    // Re-parse from disk so concurrent Code-tab edits are not clobbered.
    icg::studio::scenecpp::SceneFile file;
    std::string error;
    if (!icg::studio::scenecpp::ParseSceneFiles(currentHeader_.toStdString(),
                                                currentSource_.toStdString(), file,
                                                error)) {
        setNote(tr("Save failed, re-parse: %1").arg(QString::fromStdString(error)));
        return;
    }
    const std::string var = currentVar_.toStdString();
    // Objects need a stable file id for live addressing; assign max+1.
    uint64_t id = currentId_;
    bool haveId = currentHasId_;
    if (!haveId) {
        uint64_t maxId = 0;
        for (const auto& obj : file.model.objects) {
            if (obj.hasId && obj.id > maxId) {
                maxId = obj.id;
            }
        }
        id = maxId + 1;
        if (id == 0) {
            id = 1;
        }
        if (!icg::studio::scenecpp::SetObjectId(file, var, id, error)) {
            setNote(tr("Save failed, setId: %1").arg(QString::fromStdString(error)));
            return;
        }
        haveId = true;
    }
    const std::string px = formatDouble(posSpin_[0]->value()).toStdString();
    const std::string py = formatDouble(posSpin_[1]->value()).toStdString();
    const std::string pz = formatDouble(posSpin_[2]->value()).toStdString();
    const std::string rx = formatDouble(rotSpin_[0]->value()).toStdString();
    const std::string ry = formatDouble(rotSpin_[1]->value()).toStdString();
    const std::string rz = formatDouble(rotSpin_[2]->value()).toStdString();
    const std::string sx = formatDouble(scaleSpin_[0]->value()).toStdString();
    const std::string sy = formatDouble(scaleSpin_[1]->value()).toStdString();
    const std::string sz = formatDouble(scaleSpin_[2]->value()).toStdString();
    if (!icg::studio::scenecpp::SetTransform(file, var, "position", px, py, pz, "", error) ||
        !icg::studio::scenecpp::SetTransform(file, var, "rotation", rx, ry, rz, "", error) ||
        !icg::studio::scenecpp::SetTransform(file, var, "scale", sx, sy, sz, "", error)) {
        setNote(tr("Save failed, transform: %1").arg(QString::fromStdString(error)));
        return;
    }
    QFile out(currentSource_);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setNote(tr("Cannot write %1").arg(currentSource_));
        return;
    }
    QTextStream stream(&out);
    stream << QString::fromStdString(icg::studio::scenecpp::SerializeSource(file));
    out.close();
    currentId_ = id;
    currentHasId_ = haveId;
    applyButton_->setEnabled(connected_ && currentHasId_);
    refreshObjects();
    setNote(tr("Saved #%1 to source — rebuild + relaunch to run it.").arg(id));
}
