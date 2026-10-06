// Incogine Animator - single-instance channel (server side).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// An Animator holding a document listens on a local socket named after it (see
// `core/anim_channel.h`). When Incogine Studio opens the same file again it
// finds the socket, sends the path, and this window raises and shows the
// document instead of a second process starting up on it.
//
// Without this, double-clicking one animation twice would leave two editors
// writing the same file - the last save wins silently, which is exactly the
// kind of data loss a user blames on the tool.
#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

class AnimatorChannel : public QObject {
    Q_OBJECT

public:
    explicit AnimatorChannel(QObject* parent = nullptr);
    ~AnimatorChannel() override;

    // Starts listening for `path`. Binds the per-document channel when `path`
    // is set, and the shared session channel otherwise. Returns false when the
    // channel is already owned by another process, which is not an error.
    bool listen(const QString& path);

    // Client half: if another Animator already owns `path`, sends it there and
    // returns true so the caller can exit instead of opening a second editor.
    // Used at startup so double-clicking the same .incoanim twice (including
    // through a file association) focuses the existing window rather than
    // starting a second process on the same document.
    static bool handOffTo(const QString& path, int timeoutMs = 250);

    // Stops listening (on close, so the socket does not outlive the document).
    void close();

    bool isListening() const { return server_ != nullptr; }

    QString channelName() const;

signals:
    // Another launch asked us to open this path.
    void openPathRequested(const QString& path);

private slots:
    void onNewConnection();

private:
    QLocalServer* server_ = nullptr;
    QString channel_;
};
