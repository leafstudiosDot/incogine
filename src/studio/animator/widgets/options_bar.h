// Incogine Animator - tool options strip (brush/pen settings).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// A QToolBar shown only while a drawing tool (Brush/Pen) is active: stroke
// size, smoothing, opacity, stroke + fill pickers, and swatches. It edits the
// canvas's DrawingOptions directly and persists to QSettings, so the window
// stays out of the loop after creating it.
#pragma once

#include <QToolBar>

class AnimatorCanvas;
class QDoubleSpinBox;
class QPushButton;
class QSlider;

class OptionsBar : public QToolBar {
    Q_OBJECT

public:
    explicit OptionsBar(AnimatorCanvas* canvas, QWidget* parent = nullptr);

private slots:
    void onBrushSizeChanged(double width);
    void onSmoothingChanged(int tenths);
    void onOpacityChanged(int percent);
    void onStrokeColorPicked();
    void onFillColorPicked();
    void onSwatchClicked();

private:
    AnimatorCanvas* canvas_ = nullptr;
    QDoubleSpinBox* sizeSpin_ = nullptr;
    QSlider* smoothingSlider_ = nullptr;
    QSlider* opacitySlider_ = nullptr;
    QPushButton* strokeButton_ = nullptr;
    QPushButton* fillButton_ = nullptr;
};
