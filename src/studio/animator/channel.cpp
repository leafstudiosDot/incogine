#include "channel.h"

#include "core/anim_channel.h"

#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>

AnimatorChannel::AnimatorChannel(QObject* parent) : QObject(parent) {}

AnimatorChannel::~AnimatorChannel() {
    close();
}

bool AnimatorChannel::listen(const QString& path) {
    close();
    channel_ = path.isEmpty()
                   ? QString::fromLatin1(icg::studio::kAnimSessionChannel)
                   : QString::fromStdString(icg::studio::AnimChannelNameForPath(
                         QFileInfo(path).absoluteFilePath().toStdString()));

    // A socket left behind by a crashed previous run would make bind() fail and
    // leave this instance unable to receive hand-offs, so clear stale entries.
    QLocalServer::removeServer(channel_);
    server_ = new QLocalServer(this);
    if (!server_->listen(channel_)) {
        // Another Animator owns this channel: it already has the document, which
        // is exactly the state we wanted. Not an error.
        delete server_;
        server_ = nullptr;
        return false;
    }
    connect(server_, &QLocalServer::newConnection, this,
            &AnimatorChannel::onNewConnection);
    return true;
}

bool AnimatorChannel::handOffTo(const QString& path, int timeoutMs) {
    if (path.isEmpty()) {
        return false;
    }
    QLocalSocket socket;
    socket.connectToServer(QString::fromStdString(icg::studio::AnimChannelNameForPath(
        QFileInfo(path).absoluteFilePath().toStdString())));
    if (!socket.waitForConnected(timeoutMs)) {
        return false;
    }
    socket.write(QFileInfo(path).absoluteFilePath().toUtf8());
    socket.flush();
    socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
    return true;
}

void AnimatorChannel::close() {
    if (server_ != nullptr) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
}

QString AnimatorChannel::channelName() const {
    return channel_;
}

void AnimatorChannel::onNewConnection() {
    while (server_ != nullptr && server_->hasPendingConnections()) {
        QLocalSocket* socket = server_->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        // The peer writes the path and closes; read once the bytes arrive.
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            const QString requested = QString::fromUtf8(socket->readAll()).trimmed();
            if (requested.isEmpty()) {
                return;
            }
            emit openPathRequested(requested);
        });
    }
}