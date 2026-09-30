// Incogine Studio — manual game build runner.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Owns the cmake QProcess: configure-on-first-run, then build of the
// CMake game target only (never Studio itself, never automatic).
// Presentation (colors, button state) stays with the caller via signals.
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class CompileRunner : public QObject {
    Q_OBJECT

public:
    explicit CompileRunner(QObject* parent = nullptr);

    bool isRunning() const;
    // Starts configure (when build/CMakeCache.txt is missing) chained
    // into `cmake --build <root>/build --target <game> --config Debug`.
    // No-op while already running.
    void start(const QString& projectRoot);
    QString gameTarget() const { return target_; }

signals:
    // Raw build output lines (stdout + stderr merged in arrival order).
    void lineReceived(const QString& line);
    // Configure+build fully done; ok = exit 0 throughout (-1 = never started).
    void finished(bool ok, int exitCode);

private slots:
    void onOutput();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    static QString gameTargetName(const QString& projectRoot);
    void startBuild();

    QProcess* process_ = nullptr;
    QString root_;
    QString target_;
    QString buffer_; // partial line across readyRead chunks
    bool configuring_ = false;
};
