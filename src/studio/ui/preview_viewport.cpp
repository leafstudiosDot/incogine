// Incogine Studio — live preview monitor implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_viewport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <cstring>

#include "../core/preview/preview_exe.h"
#include "../core/xml/project_xml.h"

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

QRectF PreviewCanvas::fittedRect() const {
    if (frame_.isNull() || width() <= 0 || height() <= 0) {
        return QRectF();
    }
    const QSize fitted = frame_.size().scaled(size(), Qt::KeepAspectRatio);
    const int x = (width() - fitted.width()) / 2;
    const int y = (height() - fitted.height()) / 2;
    return QRectF(x, y, fitted.width(), fitted.height());
}

void PreviewCanvas::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (frame_.isNull()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, tr("No signal — launch Incogine preview"));
        return;
    }
    // Fit keeping aspect; flip vertically (frames arrive bottom-up).
    const QRectF fitted = fittedRect();
    painter.save();
    painter.translate(fitted.x(), fitted.y() + fitted.height());
    painter.scale(fitted.width() / frame_.width(),
                  -fitted.height() / frame_.height());
    painter.drawImage(QRect(0, 0, frame_.width(), frame_.height()), frame_);
    painter.restore();
}

// ---- PreviewViewport ----

PreviewViewport::PreviewViewport(const std::string& projectRoot, PreviewSession* session,
                                 QWidget* parent)
    : QWidget(parent), projectRoot_(projectRoot), session_(session) {
    canvas_ = new PreviewCanvas();

    exeField_ = new QLineEdit();
    exeField_->setPlaceholderText(tr("Incogine executable (auto-located from CMake)..."));
    auto* browseButton = new QPushButton(tr("Browse..."));
    launchButton_ = new QPushButton(tr("Launch preview"));
    stopButton_ = new QPushButton(tr("Stop"));
    stopButton_->setEnabled(false);
    status_ = new QLabel(tr("Stopped."));

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Incogine executable:")));
    top->addWidget(exeField_, 1);
    top->addWidget(browseButton);
    top->addWidget(launchButton_);
    top->addWidget(stopButton_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(canvas_, 1);
    layout->addWidget(status_);

    exeBaseName_ = exeBaseName();
    exeField_->setText(resolveExe());

    connect(browseButton, &QPushButton::clicked, this, &PreviewViewport::onBrowse);
    connect(launchButton_, &QPushButton::clicked, this, &PreviewViewport::onLaunch);
    connect(stopButton_, &QPushButton::clicked, this, &PreviewViewport::onStop);
    connect(session_, &PreviewSession::framesUpdated, this,
            &PreviewViewport::onSessionFrames);
    connect(session_, &PreviewSession::statusChanged, this,
            &PreviewViewport::onSessionStatus);
    connect(session_, &PreviewSession::runningChanged, this,
            &PreviewViewport::onRunningChanged);
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
    status_->setText(tr("Incogine executable: %1").arg(QString::fromStdString(error)));
    return saved;
}

void PreviewViewport::onBrowse() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Incogine executable"), exeField_->text());
    if (!path.isEmpty()) {
        exeField_->setText(path);
        QSettings settings;
        settings.setValue("previewExe", path);
    }
}

void PreviewViewport::onLaunch() {
    if (session_->isRunning()) {
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
        status_->setText(tr("Launch refused: %1")
                             .arg(QString::fromStdString(verifyError)));
        return;
    }
    QSettings settings;
    settings.setValue("previewExe", QString::fromStdString(binding.exePath));
    exeField_->setText(QString::fromStdString(binding.exePath));
    session_->start(binding.exePath.c_str(),
                    session_->makeLaunchArgs(launchScene_));
}

void PreviewViewport::onStop() {
    session_->stop();
}

void PreviewViewport::onSessionFrames() {
    canvas_->setFrame(session_->frameBytes(), session_->frameWidth(),
                      session_->frameHeight());
}

void PreviewViewport::onSessionStatus(const QString& text) {
    status_->setText(text);
}

void PreviewViewport::onRunningChanged(bool running) {
    launchButton_->setEnabled(!running);
    stopButton_->setEnabled(running);
    if (!running) {
        canvas_->clear();
    }
}
