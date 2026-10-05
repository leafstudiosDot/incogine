#include "animator_launcher.h"

#include "core/anim_channel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QProcess>

namespace {
// Executable name, with the platform's suffix (.exe on Windows).
QString animatorFileName() {
#ifdef Q_OS_WIN
    return QStringLiteral("IncogineAnimator.exe");
#else
    return QStringLiteral("IncogineAnimator");
#endif
}
} // namespace

AnimatorLauncher::AnimatorLauncher(QObject* parent) : QObject(parent) {
    locate();
}

AnimatorLauncher::~AnimatorLauncher() = default;

void AnimatorLauncher::locate() {
    if (!animatorPath_.isEmpty()) {
        return;
    }
    // Multi-config generators put the Studio binary in a config subdirectory
    // (build/studio_build/Debug/) while sibling targets land beside it, so probe
    // the executable's own directory and one/two levels up.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir,
        appDir + QStringLiteral("/.."),
        appDir + QStringLiteral("/../.."),
    };
    for (const QString& candidate : candidates) {
        const QString resolved =
            QFileInfo(candidate + QStringLiteral("/") + animatorFileName())
                .absoluteFilePath();
        if (QFileInfo::exists(resolved)) {
            animatorPath_ = resolved;
            return;
        }
    }
}

bool AnimatorLauncher::isOpen(const QString& path) {
    QLocalSocket probe;
    probe.connectToServer(QString::fromStdString(
        icg::studio::AnimChannelNameForPath(QFileInfo(path).absoluteFilePath()
                                                 .toStdString())));
    if (!probe.waitForConnected(200)) {
        return false;
    }
    probe.disconnectFromServer();
    return true;
}

bool AnimatorLauncher::open(const QString& path, QString* errorOut) {
    locate();
    if (animatorPath_.isEmpty()) {
        if (errorOut != nullptr) {
            *errorOut = QObject::tr(
                "IncogineAnimator was not found next to Incogine Studio.\n"
                "Build the IncogineAnimator target and try again.");
        }
        return false;
    }

    const QString absolute = QFileInfo(path).absoluteFilePath();
    const QString channel = QString::fromStdString(
        icg::studio::AnimChannelNameForPath(absolute.toStdString()));

    // Hand the path to an instance that already has this file open. Connecting
    // alone proves the listener exists, but writing the path also makes that
    // window actually open/raise the document.
    QLocalSocket socket;
    socket.connectToServer(channel);
    if (socket.waitForConnected(250)) {
        socket.write(absolute.toUtf8());
        socket.flush();
        socket.waitForBytesWritten(250);
        socket.disconnectFromServer();
        return true;
    }

    // Nothing listening: start a new instance, which becomes the listener.
    if (!QProcess::startDetached(animatorPath_, {absolute})) {
        if (errorOut != nullptr) {
            *errorOut = QObject::tr("Could not start %1").arg(animatorPath_);
        }
        return false;
    }
    return true;
}