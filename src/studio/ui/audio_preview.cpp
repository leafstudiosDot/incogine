// Incogine Studio - audio preview implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "audio_preview.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QShortcut>
#include <QSlider>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cstring>

// ---- WaveformView ----

WaveformView::WaveformView(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Click or drag to seek, wheel to zoom"));
}

void WaveformView::setPeaksData(const QVector<QVector<QPair<float, float>>>& peaks) {
    peaks_ = peaks;
    cacheDirty_ = true;
}

void WaveformView::setLoading(bool loading) {
    loading_ = loading;
    update();
}

void WaveformView::refresh() {
    update();
}

void WaveformView::setView(double start, double span) {
    viewStart_ = qBound(0.0, start, 1.0);
    viewSpan_ = qBound(0.0001, span, 1.0);
    if (viewStart_ + viewSpan_ > 1.0) {
        viewStart_ = 1.0 - viewSpan_;
    }
    cacheDirty_ = true;
    update();
}

void WaveformView::setPosition(qint64 positionMs, qint64 durationMs) {
    positionMs_ = positionMs;
    durationMs_ = durationMs;
    update();
}

double WaveformView::fractionAt(int x) const {
    if (width() <= 1) {
        return viewStart_;
    }
    const double local =
        static_cast<double>(x) / static_cast<double>(width() - 1);
    return qBound(0.0, viewStart_ + local * viewSpan_, 1.0);
}

void WaveformView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit seekRequested(fractionAt(qRound(event->position().x())));
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        emit seekRequested(fractionAt(qRound(event->position().x())));
    }
}

void WaveformView::wheelEvent(QWheelEvent* event) {
    const int degrees = event->angleDelta().y();
    if (degrees == 0) {
        return;
    }
    // Wheel up zooms in (factor < 1), centered on the cursor.
    emit zoomRequested(std::pow(1.0015, -degrees),
                       fractionAt(qRound(event->position().x())));
}

void WaveformView::rebuildCache() {
    cacheWidth_ = width();
    cacheDirty_ = false;
    columns_.clear();
    // ~2 columns per pixel, bounded: the "shrunk single wave" the paint
    // step then reads with one lookup per x instead of rescanning peaks.
    const int resolution = qBound(512, width() * 2, 8192);
    int rawCount = 0;
    for (const auto& lane : peaks_) {
        rawCount = qMax(rawCount, lane.size());
    }
    if (rawCount == 0) {
        return;
    }
    for (const auto& lane : peaks_) {
        QVector<QPair<float, float>> cols;
        cols.reserve(resolution);
        const int count = lane.size();
        for (int i = 0; i < resolution; ++i) {
            const double f0 = viewStart_ + static_cast<double>(i) / resolution * viewSpan_;
            const double f1 = viewStart_ + static_cast<double>(i + 1) / resolution * viewSpan_;
            int begin = qBound(0, static_cast<int>(f0 * rawCount), count - 1);
            int end = qBound(begin + 1, static_cast<int>(f1 * rawCount), count);
            float lo = 0.0f, hi = 0.0f;
            for (int k = begin; k < end; ++k) {
                lo = qMin(lo, lane[k].first);
                hi = qMax(hi, lane[k].second);
            }
            cols.append(qMakePair(lo, hi));
        }
        columns_.append(cols);
    }
}

void WaveformView::paintEvent(QPaintEvent* /*event*/) {
    if (cacheDirty_ || cacheWidth_ != width()) {
        rebuildCache();
    }
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));

    if (loading_ && columns_.isEmpty()) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(rect(), Qt::AlignCenter, tr("Loading..."));
        return;
    }

    const int lanes = qMax(1, qMin(2, static_cast<int>(columns_.size())));
    const int laneHeight = height() / lanes;
    const QColor played = palette().color(QPalette::Highlight);
    QColor rest = palette().color(QPalette::Text);
    rest.setAlpha(160);

    // Playhead x from time within the visible window.
    double playFraction = -1.0;
    if (durationMs_ > 0) {
        playFraction = static_cast<double>(positionMs_) / static_cast<double>(durationMs_);
    }
    int playX = -1;
    if (playFraction >= viewStart_ && playFraction <= viewStart_ + viewSpan_ &&
        viewSpan_ > 0.0) {
        playX = static_cast<int>((playFraction - viewStart_) / viewSpan_ * (width() - 1));
    }

    const int resolution = qMax(1, static_cast<int>(columns_.isEmpty() ? 0 : columns_.first().size()));
    for (int lane = 0; lane < lanes; ++lane) {
        const int top = lane * laneHeight;
        const int centerY = top + laneHeight / 2;
        const int amplitude = laneHeight / 2 - 2;

        // Lane separator + center line.
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawLine(0, top, width(), top);
        painter.drawLine(0, centerY, width(), centerY);

        if (lane >= columns_.size() || columns_[lane].isEmpty()) {
            continue;
        }
        const auto& cols = columns_[lane];
        for (int x = 0; x < width(); ++x) {
            const int col = qMin(static_cast<int>(static_cast<int64_t>(x) * resolution / width()),
                                 resolution - 1);
            const int y1 = centerY - static_cast<int>(cols[col].second * amplitude);
            const int y2 = centerY - static_cast<int>(cols[col].first * amplitude);
            painter.setPen(x <= playX ? played : rest);
            painter.drawLine(x, y1, x, y2 == y1 ? y1 + 1 : y2);
        }
    }

    if (playX >= 0) {
        painter.setPen(QColor(255, 64, 64));
        painter.drawLine(playX, 0, playX, height());
    }
}

// ---- AudioPreview ----

AudioPreview::AudioPreview(const QString& path, QWidget* parent)
    : QWidget(parent), filePath_(path) {
    player_ = new QMediaPlayer(this);
    audio_ = new QAudioOutput(this);
    player_->setAudioOutput(audio_);
    player_->setSource(QUrl::fromLocalFile(path));
    player_->setLoops(QMediaPlayer::Once);

    // Background decode for the waveform; playback uses the player above.
    decoder_ = new QAudioDecoder(this);
    decoder_->setSource(QUrl::fromLocalFile(path));
    connect(decoder_, &QAudioDecoder::bufferReady, this, &AudioPreview::onDecodeBuffer);
    connect(decoder_, &QAudioDecoder::finished, this, &AudioPreview::onDecodeFinished);
    connect(decoder_, static_cast<void (QAudioDecoder::*)(QAudioDecoder::Error)>(
                                &QAudioDecoder::error),
            this, &AudioPreview::onDecodeError);
    decoder_->start();

    waveform_ = new WaveformView();

    playButton_ = new QPushButton(tr("Play"));
    playButton_->setFixedWidth(80);
    playButton_->setFocusPolicy(Qt::NoFocus);
    // Playback unlocks once the waveform has loaded (or failed to -
    // the player backend is independent of the decoder).
    playButton_->setEnabled(false);
    timeLabel_ = new QLabel(tr("00:00 / 00:00"));
    timeLabel_->setMinimumWidth(130);

    auto* zoomOut = new QPushButton(tr("-"));
    zoomOut->setFixedWidth(32);
    zoomOut->setToolTip(tr("Zoom out"));
    auto* zoomIn = new QPushButton(tr("+"));
    zoomIn->setFixedWidth(32);
    zoomIn->setToolTip(tr("Zoom in"));
    auto* zoomFit = new QPushButton(tr("Fit"));
    zoomFit->setFixedWidth(48);
    zoomFit->setToolTip(tr("Show whole file"));

    volumeSlider_ = new QSlider(Qt::Horizontal);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(100);
    volumeSlider_->setFixedWidth(100);
    volumeSlider_->setToolTip(tr("Volume"));
    muteButton_ = new QPushButton(tr("Mute"));
    muteButton_->setCheckable(true);
    muteButton_->setFixedWidth(80);
    muteButton_->setFocusPolicy(Qt::NoFocus);
    loopButton_ = new QPushButton(tr("Loop"));
    loopButton_->setCheckable(true);
    loopButton_->setFixedWidth(80);
    loopButton_->setFocusPolicy(Qt::NoFocus);

    auto* playShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    playShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(playShortcut, &QShortcut::activated, this, &AudioPreview::togglePlay);

    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(playButton_);
    row->addWidget(timeLabel_);
    row->addStretch(1);
    row->addWidget(zoomOut);
    row->addWidget(zoomIn);
    row->addWidget(zoomFit);
    row->addWidget(new QLabel(tr("Vol")));
    row->addWidget(volumeSlider_);
    row->addWidget(muteButton_);
    row->addWidget(loopButton_);

    infoLabel_ = new QLabel();
    infoLabel_->setStyleSheet("color: palette(mid);");
    statusLabel_ = new QLabel(tr("Decoding waveform..."));
    statusLabel_->setStyleSheet("color: palette(mid);");
    QHBoxLayout* infoRow = new QHBoxLayout();
    infoRow->addWidget(infoLabel_);
    infoRow->addStretch(1);
    infoRow->addWidget(statusLabel_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(path));
    layout->addWidget(waveform_, 1);
    layout->addLayout(row);
    layout->addLayout(infoRow);
    refreshInfo();

    connect(playButton_, &QPushButton::clicked, this, &AudioPreview::togglePlay);
    connect(player_, &QMediaPlayer::playbackStateChanged,
            this, &AudioPreview::onPlaybackStateChanged);
    connect(player_, &QMediaPlayer::positionChanged,
            this, &AudioPreview::onPositionChanged);
    connect(player_, &QMediaPlayer::durationChanged,
            this, &AudioPreview::onDurationChanged);
    connect(waveform_, &WaveformView::seekRequested, this, &AudioPreview::onWaveformSeek);
    connect(waveform_, &WaveformView::zoomRequested, this, &AudioPreview::onWaveformZoom);
    connect(zoomOut, &QPushButton::clicked, this, &AudioPreview::onZoomOut);
    connect(zoomIn, &QPushButton::clicked, this, &AudioPreview::onZoomIn);
    connect(zoomFit, &QPushButton::clicked, this, &AudioPreview::onZoomFit);
    connect(loopButton_, &QPushButton::toggled, this, &AudioPreview::onLoopToggled);
    connect(volumeSlider_, &QSlider::valueChanged, this, &AudioPreview::onVolumeChanged);
    connect(muteButton_, &QPushButton::toggled, this, &AudioPreview::onMuteToggled);
}

AudioPreview::~AudioPreview() {
    delete decodedFormat_;
}

QString AudioPreview::fmtTime(qint64 ms) {
    const qint64 totalSeconds = ms / 1000;
    return QString("%1:%2")
        .arg(totalSeconds / 60, 2, 10, QChar('0'))
        .arg(totalSeconds % 60, 2, 10, QChar('0'));
}

QString AudioPreview::formatInfo(const QAudioFormat& format) {
    const double khz = format.sampleRate() / 1000.0;
    const QString channels = format.channelCount() >= 2 ? tr("Stereo")
                           : format.channelCount() == 1 ? tr("Mono")
                           : tr("%1 ch").arg(format.channelCount());
    QString bits;
    switch (format.sampleFormat()) {
        case QAudioFormat::Float: bits = tr("32-bit float"); break;
        case QAudioFormat::Int32: bits = tr("32-bit"); break;
        case QAudioFormat::Int16: bits = tr("16-bit"); break;
        case QAudioFormat::UInt8: bits = tr("8-bit"); break;
        default: bits = tr("unknown"); break;
    }
    return tr("%1 kHz - %2 - %3").arg(QString::number(khz, 'f', 1)).arg(channels).arg(bits);
}

void AudioPreview::togglePlay() {
    if (!playButton_->isEnabled()) {
        return; // still loading (or load failed silently); see status label
    }
    if (player_->playbackState() == QMediaPlayer::PlayingState) {
        player_->pause();
    } else {
        player_->play();
    }
}

void AudioPreview::onPlaybackStateChanged() {
    playButton_->setText(player_->playbackState() == QMediaPlayer::PlayingState
                             ? tr("Pause")
                             : tr("Play"));
}

void AudioPreview::onPositionChanged(qint64 ms) {
    timeLabel_->setText(tr("%1 / %2").arg(fmtTime(ms), fmtTime(player_->duration())));
    waveform_->setPosition(ms, player_->duration());
}

void AudioPreview::onDurationChanged(qint64 ms) {
    timeLabel_->setText(tr("%1 / %2").arg(fmtTime(player_->position()), fmtTime(ms)));
    waveform_->setPosition(player_->position(), ms);
    refreshInfo();
}

void AudioPreview::onWaveformSeek(double fraction) {
    const qint64 duration = player_->duration();
    if (duration > 0) {
        player_->setPosition(static_cast<qint64>(fraction * duration));
    }
}

void AudioPreview::onWaveformZoom(double factor, double center) {
    const double oldSpan = viewSpan_;
    const double newSpan = qBound(1.0 / 128.0, oldSpan * factor, 1.0);
    if (qFuzzyCompare(newSpan, oldSpan)) {
        return;
    }
    viewStart_ = center - (center - viewStart_) * newSpan / oldSpan;
    viewStart_ = qBound(0.0, viewStart_, 1.0 - newSpan);
    viewSpan_ = newSpan;
    applyView();
}

void AudioPreview::onZoomOut() {
    onWaveformZoom(2.0, viewStart_ + viewSpan_ / 2.0);
}

void AudioPreview::onZoomIn() {
    onWaveformZoom(0.5, viewStart_ + viewSpan_ / 2.0);
}

void AudioPreview::onZoomFit() {
    viewStart_ = 0.0;
    viewSpan_ = 1.0;
    applyView();
}

void AudioPreview::applyView() {
    waveform_->setView(viewStart_, viewSpan_);
}

void AudioPreview::onLoopToggled(bool on) {
    player_->setLoops(on ? QMediaPlayer::Infinite : QMediaPlayer::Once);
}

void AudioPreview::onVolumeChanged(int percent) {
    audio_->setVolume(static_cast<float>(percent) / 100.0f);
    if (percent > 0 && muteButton_->isChecked()) {
        muteButton_->setChecked(false);
    }
}

void AudioPreview::onMuteToggled(bool on) {
    audio_->setMuted(on);
}

namespace {

float sampleToFloat(const char* frames, int frame, int channel,
                    QAudioFormat::SampleFormat format, int bytesPerFrame,
                    int bytesPerSample) {
    const char* sample = frames + frame * bytesPerFrame + channel * bytesPerSample;
    switch (format) {
        case QAudioFormat::Float: {
            float v = 0.0f;
            std::memcpy(&v, sample, sizeof(v));
            return v;
        }
        case QAudioFormat::Int32: {
            qint32 v = 0;
            std::memcpy(&v, sample, sizeof(v));
            return static_cast<float>(v) / 2147483648.0f;
        }
        case QAudioFormat::Int16: {
            qint16 v = 0;
            std::memcpy(&v, sample, sizeof(v));
            return static_cast<float>(v) / 32768.0f;
        }
        case QAudioFormat::UInt8: {
            return (static_cast<uint8_t>(*sample) - 128) / 128.0f;
        }
        default:
            return 0.0f;
    }
}

} // namespace

void AudioPreview::onDecodeBuffer() {
    const QAudioBuffer buffer = decoder_->read();
    if (!buffer.isValid() || buffer.frameCount() <= 0) {
        return;
    }
    if (!decodedFormat_) {
        decodedFormat_ = new QAudioFormat(buffer.format());
        refreshInfo();
    }
    const QAudioFormat format = buffer.format();
    const int channels = qMin(2, format.channelCount());
    const int frames = buffer.frameCount();
    const char* data = buffer.constData<char>();
    while (peaks_.size() < channels) {
        peaks_.append(QVector<QPair<float, float>>());
    }
    for (int ch = 0; ch < channels; ++ch) {
        float lo = 0.0f, hi = 0.0f;
        for (int f = 0; f < frames; ++f) {
            const float v = sampleToFloat(data, f, ch, format.sampleFormat(),
                                          format.bytesPerFrame(), format.bytesPerSample());
            lo = qMin(lo, v);
            hi = qMax(hi, v);
        }
        peaks_[ch].append(qMakePair(lo, hi));
    }
    // Decode buffers arrive in bursts; redrawing every one rescans and
    // repaints for no visible gain. Cap waveform draws at ~8 Hz; the
    // finished handler draws the final state.
    waveform_->setPeaksData(peaks_);
    if (!waveClock_.isValid() || waveClock_.elapsed() > 120) {
        waveform_->refresh();
        waveClock_.restart();
    }
}

void AudioPreview::onDecodeFinished() {
    statusLabel_->setText(QString());
    waveform_->setLoading(false);
    playButton_->setEnabled(true);
    waveClock_.invalidate();
    refreshWaveform();
    refreshInfo();
}

void AudioPreview::onDecodeError(QAudioDecoder::Error /*error*/) {
    statusLabel_->setText(tr("Waveform unavailable: %1").arg(decoder_->errorString()));
    waveform_->setLoading(false);
    playButton_->setEnabled(true);
}

void AudioPreview::refreshWaveform() {
    waveform_->setPeaksData(peaks_);
    waveform_->refresh();
}

void AudioPreview::refreshInfo() {
    const QFileInfo info(filePath_);
    const double mb = info.size() / (1024.0 * 1024.0);
    QString text = tr("%1 MB").arg(QString::number(mb, 'f', 1));
    if (decodedFormat_) {
        text += tr(" - %1").arg(formatInfo(*decodedFormat_));
    }
    const qint64 duration = player_->duration();
    if (duration > 0) {
        text += tr(" - %1").arg(fmtTime(duration));
    }
    infoLabel_->setText(text);
}

