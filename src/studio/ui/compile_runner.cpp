// Incogine Studio - manual game build runner implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "compile_runner.h"

#include <QFile>

CompileRunner::CompileRunner(QObject* parent) : QObject(parent) {
    process_ = new QProcess(this);
    connect(process_, &QProcess::readyReadStandardOutput, this,
            &CompileRunner::onOutput);
    connect(process_, &QProcess::readyReadStandardError, this,
            &CompileRunner::onOutput);
    connect(process_, &QProcess::errorOccurred, this,
            &CompileRunner::onProcessError);
    connect(process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &CompileRunner::onProcessFinished);
}

bool CompileRunner::isRunning() const {
    return process_ && process_->state() != QProcess::NotRunning;
}

QString CompileRunner::gameTargetName(const QString& projectRoot) {
    // Desktop game target == the CMake top-level project name. Read it from
    // the configured build tree so a renamed project keeps working.
    QFile cache(projectRoot + QStringLiteral("/build/CMakeCache.txt"));
    if (cache.open(QIODevice::ReadOnly)) {
        static const QString key = QStringLiteral("CMAKE_PROJECT_NAME:STATIC=");
        while (!cache.atEnd()) {
            const QString line = QString::fromUtf8(cache.readLine());
            if (line.startsWith(key)) {
                const QString name = line.mid(key.size()).trimmed();
                if (!name.isEmpty()) {
                    return name;
                }
            }
        }
    }
    return QStringLiteral("Incogine");
}

void CompileRunner::start(const QString& projectRoot) {
    if (isRunning()) {
        return;
    }
    root_ = projectRoot;
    target_ = gameTargetName(root_);
    buffer_.clear();
    const QString buildPath = root_ + QStringLiteral("/build");
    if (!QFile::exists(buildPath + QStringLiteral("/CMakeCache.txt"))) {
        // First run: configure with the default generator (newest VS on
        // Windows), then chain into the build when it succeeds.
        configuring_ = true;
        emit lineReceived(tr("Configuring %1 ...").arg(buildPath));
        process_->setProgram(QStringLiteral("cmake"));
        process_->setArguments(
            {QStringLiteral("-S"), root_, QStringLiteral("-B"), buildPath});
    } else {
        configuring_ = false;
        startBuild();
    }
    process_->start();
}

void CompileRunner::startBuild() {
    const QString buildPath = root_ + QStringLiteral("/build");
    emit lineReceived(tr("Building target %1 (Debug) ...").arg(target_));
    process_->setProgram(QStringLiteral("cmake"));
    process_->setArguments({QStringLiteral("--build"), buildPath,
                            QStringLiteral("--target"), target_,
                            QStringLiteral("--config"), QStringLiteral("Debug")});
}

void CompileRunner::onOutput() {
    if (!process_) {
        return;
    }
    buffer_ += QString::fromLocal8Bit(process_->readAllStandardOutput());
    buffer_ += QString::fromLocal8Bit(process_->readAllStandardError());
    int nl = -1;
    while ((nl = buffer_.indexOf(QLatin1Char('\n'))) >= 0) {
        QString line = buffer_.left(nl);
        buffer_ = buffer_.mid(nl + 1);
        if (!line.isEmpty() && line.back() == QLatin1Char('\r')) {
            line.chop(1);
        }
        emit lineReceived(line);
    }
}

void CompileRunner::onProcessError(QProcess::ProcessError error) {
    if (process_ && process_->state() == QProcess::NotRunning) {
        emit lineReceived(
            tr("Could not start cmake (error %1). Is CMake on PATH?")
                .arg(static_cast<int>(error)));
        configuring_ = false;
        emit finished(false, -1);
    }
}

void CompileRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    if (!buffer_.isEmpty()) {
        emit lineReceived(buffer_);
        buffer_.clear();
    }
    const bool ok = (status == QProcess::NormalExit && exitCode == 0);
    if (configuring_) {
        configuring_ = false;
        if (ok) {
            startBuild(); // chain configure -> build
            process_->start();
            return;
        }
        emit lineReceived(tr("Configure failed (exit %1).").arg(exitCode));
        emit finished(false, exitCode);
        return;
    }
    emit finished(ok, exitCode);
}

