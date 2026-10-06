// Incogine Studio - launch the Incogine Animator on a .incoanim file.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Incogine Animator is a separate executable, so opening one is a process launch,
// not a widget creation. Two consequences drive the design:
//
//   * Animator outlives Studio. Double-clicking three animations starts three
//     independent processes; closing Studio leaves them running, which is the
//     point of keeping the editor separate.
//   * The same file opened twice routes to the already-running editor instead of
//     producing a second window on the same document. Studio sends the path over
//     a local socket; when nothing answers, it starts a new process.
//
// This class is the CLIENT half only. The listening half lives in the Animator
// itself (`src/studio/animator/animator_channel.*`), and both derive the socket
// name from `anim_channel.h` so they always agree.
#pragma once

#include <QObject>
#include <QString>

class AnimatorLauncher : public QObject {
    Q_OBJECT

public:
    explicit AnimatorLauncher(QObject* parent = nullptr);
    ~AnimatorLauncher() override;

    // Absolute path to the Animator executable, empty when not found.
    QString animatorPath() const { return animatorPath_; }

    // Finds IncogineAnimator near this Studio binary (same directory, or one or
    // two levels up for multi-config layouts like Debug/Release).
    void locate();

    // True when an Animator is already listening for `path` - used by the
    // self-test and to report "opened in the existing window" in the log.
    bool isOpen(const QString& path);

    // Opens `path` in the Animator, handing off to a running instance when there
    // is one. Returns false with `errorOut` when the executable is missing or
    // the launch fails.
    bool open(const QString& path, QString* errorOut = nullptr);

private:
    QString animatorPath_;
};
