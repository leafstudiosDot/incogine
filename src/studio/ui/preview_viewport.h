// Incogine Studio — live preview monitor tab (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Passive view over the shared PreviewSession: game picker with dev-build
// binding, launch/stop, live frames, status. Scene selection and all
// editing live in the Scene editor tab; this tab only runs and watches.
#pragma once

#include <QWidget>

#include <string>

#include "preview_session.h"

class QLabel;
class QLineEdit;
class QPushButton;

class PreviewCanvas : public QWidget {
    Q_OBJECT

public:
    explicit PreviewCanvas(QWidget* parent = nullptr);

    void setFrame(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height);
    void clear();

    // Aspect-fit content rect (same math as the paint step) for overlays
    // and coordinate mapping.
    QRectF fittedRect() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    QImage frame_;
};

class PreviewViewport : public QWidget {
    Q_OBJECT

public:
    explicit PreviewViewport(const std::string& projectRoot, PreviewSession* session,
                             QWidget* parent = nullptr);

    // Scene class booted on launch (empty = default boot). Set by the main
    // window from the Scene tab selection.
    void setLaunchScene(const QString& className) { launchScene_ = className; }

private slots:
    void onLaunch();
    void onStop();
    void onBrowse();
    void onSessionFrames();
    void onSessionStatus(const QString& text);
    void onRunningChanged(bool running);

private:
    QString resolveExe();
    QString exeBaseName();

    std::string projectRoot_;
    PreviewSession* session_ = nullptr;
    QString exeBaseName_;
    QString launchScene_;
    PreviewCanvas* canvas_ = nullptr;
    QLabel* status_ = nullptr;
    QLineEdit* exeField_ = nullptr;
    QPushButton* launchButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
};
