// Incogine Studio — shared preview session implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_session.h"

#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QTimer>

#include "../../core/preview/preview_protocol.h"

PreviewSession::PreviewSession(QObject* parent) : QObject(parent) {
    proc_ = new QProcess(this);
    poll_ = new QTimer(this);
    poll_->setInterval(33); // ~30 Hz display polling
    connect(poll_, &QTimer::timeout, this, &PreviewSession::onPoll);
    connect(proc_, &QProcess::finished, this, &PreviewSession::onProcessFinished);
}

bool PreviewSession::isRunning() const {
    return proc_->state() != QProcess::NotRunning;
}

bool PreviewSession::isConnected() const {
    return connected_;
}

QStringList PreviewSession::makeLaunchArgs(const QString& sceneClass) {
    sessionToken_ = 0;
    while (sessionToken_ == 0) {
        sessionToken_ = QRandomGenerator::global()->generate64();
    }
    QStringList args;
    if (sceneClass.isEmpty()) {
        args << "--studio-preview";
    } else {
        args << ("--studio-preview=" + sceneClass);
    }
    args << ("--studio-token=" + QString::number(sessionToken_, 16));
    return args;
}

void PreviewSession::start(const QString& exe, const QStringList& args) {
    if (isRunning()) {
        return;
    }
    client_.Disconnect();
    connected_ = false;
    emit connectedChanged(false);
    proc_->setWorkingDirectory(QFileInfo(exe).absolutePath());
    proc_->start(exe, args);
    if (!proc_->waitForStarted(5000)) {
        emit statusChanged(tr("Failed to start the game process."));
        return;
    }
    emit runningChanged(true);
    poll_->start();
    emit statusChanged(tr("Game started — waiting for frames..."));
}

void PreviewSession::stop() {
    if (!isRunning()) {
        return;
    }
    std::string error;
    if (connected_ && !client_.SendCommand(ICG_PREVIEW_CMD_QUIT, 3000, error)) {
        emit statusChanged(tr("Engine did not acknowledge quit — terminating."));
    }
    proc_->terminate();
    if (!proc_->waitForFinished(3000)) {
        proc_->kill();
    }
}

bool PreviewSession::sendTransform(uint64_t id, const float pos[3], const float rot[3],
                                   const float scale[3], std::string& error) {
    if (!connected_) {
        error = "preview not connected";
        return false;
    }
    return client_.SendTransform(id, pos, rot, scale, 2000, error);
}

void PreviewSession::onPoll() {
    if (!isRunning()) {
        return; // onProcessFinished reports it
    }
    if (!connected_) {
        std::string error;
        if (client_.Connect("incogine_preview", sessionToken_, error)) {
            connected_ = true;
            emit connectedChanged(true);
        } else if (!error.empty() && error.find("token mismatch") != std::string::npos) {
            emit statusChanged(
                tr("Session token mismatch — not the build Studio launched."));
            stop();
        }
        return;
    }
    uint32_t width = 0, height = 0, seq = 0;
    if (client_.TryFrame(frameBytes_, width, height, seq)) {
        frameWidth_ = width;
        frameHeight_ = height;
        frameSeq_ = seq;
        emit framesUpdated();
        emit statusChanged(tr("Live %1x%2 #%3").arg(width).arg(height).arg(seq));
    }
}

void PreviewSession::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    poll_->stop();
    client_.Disconnect();
    connected_ = false;
    emit connectedChanged(false);
    emit runningChanged(false);
    emit statusChanged(status == QProcess::NormalExit
                           ? tr("Game exited (code %1).").arg(exitCode)
                           : tr("Game process crashed."));
}
