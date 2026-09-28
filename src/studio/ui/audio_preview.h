// Incogine Studio — audio preview tab (Qt Widgets + Qt Multimedia).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Adobe Audition-style layout: stereo waveform lanes on top (decoded in the
// background, drawn progressively), transport controls underneath —
// play/pause, duration readout, volume/mute, zoom, loop toggle. Clicking or
// dragging on the waveform seeks immediately; the wheel zooms. Only built
// when Qt6 Multimedia was found (ICG_STUDIO_HAS_MULTIMEDIA).
#pragma once

#include <QPair>
#include <QVector>
#include <QWidget>
#include <QAudioDecoder>
#include <QElapsedTimer>

class QLabel;
class QMediaPlayer;
class QPushButton;
class QSlider;
class QAudioFormat;
class QAudioOutput;

// Per-buffer (min, max) peak pairs, one vector per channel. The view maps a
// visible window [viewStart, viewStart + viewSpan) of buffer-index fractions
// to x, so partial decodes and zoom both draw correctly without knowing the
// total duration upfront.
class WaveformView : public QWidget {
    Q_OBJECT

public:
    explicit WaveformView(QWidget* parent = nullptr);

    // Stores raw peaks and marks the column cache dirty (no repaint).
    void setPeaksData(const QVector<QVector<QPair<float, float>>>& peaks);
    // Loading state: shows a centered "Loading..." until the first peaks
    // arrive; playback stays disabled meanwhile (see AudioPreview).
    void setLoading(bool loading);
    // Repaints (rebuilds the column cache first if dirty).
    void refresh();
    void setView(double start, double span);
    void setPosition(qint64 positionMs, qint64 durationMs);

signals:
    void seekRequested(double fraction); // absolute 0.0 - 1.0, from click/drag
    void zoomRequested(double factor, double center); // wheel, <1 zooms in

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    double fractionAt(int x) const;
    // Shrinks raw peaks into fixed-resolution columns for the current view
    // ("one wave"): repaints then cost O(width) lookups instead of a rescan.
    void rebuildCache();

    QVector<QVector<QPair<float, float>>> peaks_;
    QVector<QVector<QPair<float, float>>> columns_;
    int cacheWidth_ = 0;
    bool cacheDirty_ = true;
    bool loading_ = true;
    double viewStart_ = 0.0;
    double viewSpan_ = 1.0;
    qint64 positionMs_ = 0;
    qint64 durationMs_ = 0;
};

class AudioPreview : public QWidget {
    Q_OBJECT

public:
    explicit AudioPreview(const QString& path, QWidget* parent = nullptr);
    ~AudioPreview() override;

private slots:
    void togglePlay();
    void onPlaybackStateChanged();
    void onPositionChanged(qint64 ms);
    void onDurationChanged(qint64 ms);
    void onWaveformSeek(double fraction);
    void onWaveformZoom(double factor, double center);
    void onZoomOut();
    void onZoomIn();
    void onZoomFit();
    void onLoopToggled(bool on);
    void onVolumeChanged(int percent);
    void onMuteToggled(bool on);
    void onDecodeBuffer();
    void onDecodeFinished();
    void onDecodeError(QAudioDecoder::Error error);

private:
    static QString fmtTime(qint64 ms);
    static QString formatInfo(const QAudioFormat& format);
    void refreshWaveform();
    void refreshInfo();
    void applyView();

    QMediaPlayer* player_;
    QAudioOutput* audio_;
    QAudioDecoder* decoder_;
    WaveformView* waveform_;
    QPushButton* playButton_;
    QLabel* timeLabel_;
    QPushButton* loopButton_;
    QSlider* volumeSlider_;
    QPushButton* muteButton_;
    QLabel* infoLabel_;
    QLabel* statusLabel_;
    QString filePath_;
    QAudioFormat* decodedFormat_ = nullptr;
    double viewStart_ = 0.0;
    double viewSpan_ = 1.0;
    QElapsedTimer waveClock_; // throttles waveform redraws during decode
    // Raw per-buffer peaks per channel (owns the data; the view borrows it).
    QVector<QVector<QPair<float, float>>> peaks_;
};
