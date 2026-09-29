// Incogine Studio — shared preview session (Qt Widgets + Core client).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Owns the single preview game process and its channel: launch/stop,
// ~30 Hz frame polling, transform sends. Shared by the Scene editor tab
// (interactive view) and the Preview tab (passive monitor), so the two
// can never fight over the single-instance SHM segment. Exe resolution
// and dev-build binding stay with the launching UI, not here.
#pragma once

#include <QObject>
#include <QProcess>

#include <cstdint>
#include <string>
#include <vector>

#include "../core/preview/preview_client.h"

class QTimer;

class PreviewSession : public QObject {
    Q_OBJECT

public:
    explicit PreviewSession(QObject* parent = nullptr);

    bool isRunning() const;
    bool isConnected() const;

    // Generates a fresh session token and builds the launch arguments
    // (bare --studio-preview when sceneClass is empty).
    QStringList makeLaunchArgs(const QString& sceneClass);
    uint64_t sessionToken() const { return sessionToken_; }

    const std::vector<uint8_t>& frameBytes() const { return frameBytes_; }
    uint32_t frameWidth() const { return frameWidth_; }
    uint32_t frameHeight() const { return frameHeight_; }
    uint32_t frameSeq() const { return frameSeq_; }

public slots:
    void start(const QString& exe, const QStringList& args);
    void stop();
    bool sendTransform(uint64_t id, const float pos[3], const float rot[3],
                       const float scale[3], std::string& error);

signals:
    void framesUpdated();
    void statusChanged(const QString& text);
    void runningChanged(bool running);
    void connectedChanged(bool connected);

private slots:
    void onPoll();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess* proc_ = nullptr;
    QTimer* poll_ = nullptr;
    icg::studio::preview::PreviewClient client_;
    bool connected_ = false;
    uint64_t sessionToken_ = 0;
    std::vector<uint8_t> frameBytes_;
    uint32_t frameWidth_ = 0;
    uint32_t frameHeight_ = 0;
    uint32_t frameSeq_ = 0;
};
