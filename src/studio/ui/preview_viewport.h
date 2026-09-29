// Incogine Studio — live preview viewport tab (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Shows the exact frames of a game running with --studio-preview (same
// renderer, same design resolution) and manages its lifetime: launch the
// game build as a subprocess, poll frames, ask it to quit on stop.
//
// Below the viewport: scene picker (boots that scene in the game),
// parser-backed object list for the chosen scene, and Position/Rotation/
// Scale controls with two destinations — "Apply live" moves the running
// object over the preview channel (needs a file id), "Save to source"
// rewrites the .cpp via the round-trip parser (assigning an id first when
// missing) so the change survives rebuilds.
#pragma once

#include <QWidget>
#include <QImage>
#include <QMap>
#include <QProcess>

#include <cstdint>
#include <string>
#include <vector>

#include "../core/preview_client.h"
#include "../core/scene_cpp.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;

// Displays bottom-up RGBA frames (GL row order) with a vertical flip done
// in the painter, so no per-frame copy is needed for orientation.
class PreviewCanvas : public QWidget {
    Q_OBJECT

public:
    explicit PreviewCanvas(QWidget* parent = nullptr);

    void setFrame(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage frame_;
};

class PreviewViewport : public QWidget {
    Q_OBJECT

public:
    explicit PreviewViewport(const std::string& projectRoot, QWidget* parent = nullptr);

    // Test/introspection: item data carries the scene class name (used for
    // the --studio-preview= argument); header/source paths live in scenePaths_.
    bool selectPreviewScene(const QString& className);
    QString selectedPreviewScene() const;

private slots:
    void onLaunch();
    void onStop();
    void onBrowse();
    void onPoll();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onSceneChanged(int index);
    void onObjectSelected();
    void onApplyLive();
    void onSaveToSource();

private:
    void setStatus(const QString& text);
    QString resolveExe();
    QString exeBaseName();
    void refreshScenes();
    void refreshObjects();
    void setNote(const QString& text);

    QProcess* proc_ = nullptr;
    QTimer* poll_ = nullptr;
    PreviewCanvas* canvas_ = nullptr;
    QLabel* status_ = nullptr;
    QLineEdit* exeField_ = nullptr;
    QPushButton* launchButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QComboBox* sceneCombo_ = nullptr;
    QMap<QString, QStringList> scenePaths_; // class name -> {header, source}
    QListWidget* objectList_ = nullptr;
    QLabel* noteLabel_ = nullptr;
    QDoubleSpinBox* posSpin_[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* rotSpin_[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* scaleSpin_[3] = {nullptr, nullptr, nullptr};
    QPushButton* applyButton_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    icg::studio::preview::PreviewClient client_;
    bool connected_ = false;
    uint64_t sessionToken_ = 0;
    std::vector<uint8_t> frameBytes_;
    std::string projectRoot_;
    QString exeBaseName_;
    icg::studio::scenecpp::SceneFile currentScene_;
    bool currentSceneOk_ = false;
    QString currentHeader_;
    QString currentSource_;
    QString currentVar_;
    uint64_t currentId_ = 0;
    bool currentHasId_ = false;
};
