#include "widgets/options_bar.h"

#include <QApplication>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSlider>

#include "canvas.h"

namespace {
// 12 swatches, Flash-style basics. Stored as #RRGGBB; alpha comes from the
// picked color * opacity, not the swatch.
const char* kSwatches[] = {
    "#000000", "#FFFFFF", "#FF0000", "#FF8000", "#FFFF00", "#00FF00",
    "#00FFFF", "#0000FF", "#8000FF", "#FF00FF", "#808080", "#8B4513",
};

QIcon colorIcon(const QColor& color) {
    QPixmap pix(22, 22);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setPen(QColor(90, 90, 90));
    painter.setBrush(color);
    painter.drawRect(1, 1, 20, 20);
    return QIcon(pix);
}

QColor toQColor(const icg::anim::AnimColor& color) {
    return QColor(color.r, color.g, color.b, color.a);
}

icg::anim::AnimColor toAnimColor(const QColor& color) {
    return icg::anim::AnimColor(color.red(), color.green(), color.blue(),
                                color.alpha());
}
} // namespace

OptionsBar::OptionsBar(AnimatorCanvas* canvas, QWidget* parent)
    : QToolBar(parent), canvas_(canvas) {
    setObjectName(QStringLiteral("animatorOptionsBar"));
    setMovable(false);
    setWindowTitle(tr("Tool Options"));

    QSettings settings;
    const double savedWidth =
        settings.value(QStringLiteral("animator/brush/size"), 4.0).toDouble();
    const double savedSmooth =
        settings.value(QStringLiteral("animator/brush/smoothing"), 1.5).toDouble();
    const int savedOpacity =
        settings.value(QStringLiteral("animator/brush/opacity"), 100).toInt();
    const QColor savedStroke = QColor(
        settings.value(QStringLiteral("animator/strokeColor"), "#000000")
            .toString());
    const QColor savedFill = QColor(
        settings.value(QStringLiteral("animator/fillColor"), "#FFFFFF")
            .toString());

    auto* sizeLabel = new QLabel(tr("Size"), this);
    sizeSpin_ = new QDoubleSpinBox(this);
    sizeSpin_->setRange(0.5, 100.0);
    sizeSpin_->setDecimals(1);
    sizeSpin_->setSingleStep(1.0);
    sizeSpin_->setSuffix(tr(" px"));
    sizeSpin_->setValue(savedWidth);
    sizeSpin_->setToolTip(tr("Brush / pen stroke width in stage units"));
    connect(sizeSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OptionsBar::onBrushSizeChanged);

    auto* smoothLabel = new QLabel(tr("Smoothing"), this);
    smoothingSlider_ = new QSlider(Qt::Horizontal, this);
    smoothingSlider_->setRange(0, 80); // tenths of a stage unit, 0..8
    smoothingSlider_->setValue(static_cast<int>(savedSmooth * 10.0));
    smoothingSlider_->setFixedWidth(110);
    smoothingSlider_->setToolTip(
        tr("Brush smoothing: RDP tolerance in stage units"));
    connect(smoothingSlider_, &QSlider::valueChanged, this,
            &OptionsBar::onSmoothingChanged);

    auto* opacityLabel = new QLabel(tr("Opacity"), this);
    opacitySlider_ = new QSlider(Qt::Horizontal, this);
    opacitySlider_->setRange(5, 100);
    opacitySlider_->setValue(savedOpacity);
    opacitySlider_->setFixedWidth(90);
    opacitySlider_->setToolTip(tr("Stroke opacity"));
    connect(opacitySlider_, &QSlider::valueChanged, this,
            &OptionsBar::onOpacityChanged);

    strokeButton_ = new QPushButton(tr("Stroke"), this);
    strokeButton_->setToolTip(tr("Stroke color (brush and pen)"));
    connect(strokeButton_, &QPushButton::clicked, this,
            &OptionsBar::onStrokeColorPicked);
    fillButton_ = new QPushButton(tr("Fill"), this);
    fillButton_->setToolTip(
        tr("Fill color (reserved for future shape tools - brush dots use the stroke color)"));
    connect(fillButton_, &QPushButton::clicked, this,
            &OptionsBar::onFillColorPicked);

    addWidget(sizeLabel);
    addWidget(sizeSpin_);
    addSeparator();
    addWidget(smoothLabel);
    addWidget(smoothingSlider_);
    addSeparator();
    addWidget(opacityLabel);
    addWidget(opacitySlider_);
    addSeparator();
    addWidget(strokeButton_);
    addWidget(fillButton_);
    addSeparator();

    // Swatches: click sets the stroke, Alt+click sets the fill. One row keeps
    // the strip compact; the tooltip states the modifier so it is discoverable.
    for (const char* hex : kSwatches) {
        const QColor swatch(hex);
        auto* button = new QPushButton(this);
        button->setIcon(colorIcon(swatch));
        button->setFixedSize(28, 24);
        button->setProperty("swatch", swatch.name());
        button->setToolTip(
            tr("%1  (click: stroke, Alt+click: fill)").arg(swatch.name()));
        connect(button, &QPushButton::clicked, this,
                &OptionsBar::onSwatchClicked);
        addWidget(button);
    }

    // Push the saved values into the canvas before first paint.
    if (canvas_ != nullptr) {
        canvas_->setStrokeWidth(static_cast<float>(savedWidth));
        canvas_->setSmoothing(static_cast<float>(savedSmooth));
        canvas_->setStrokeOpacity(savedOpacity / 100.0f);
        canvas_->setStrokeColor(toAnimColor(savedStroke));
        canvas_->setFillColor(toAnimColor(savedFill));
    }
    strokeButton_->setIcon(colorIcon(savedStroke));
    fillButton_->setIcon(colorIcon(savedFill));
}

void OptionsBar::onBrushSizeChanged(double width) {
    if (canvas_ != nullptr) {
        canvas_->setStrokeWidth(static_cast<float>(width));
    }
    QSettings settings;
    settings.setValue(QStringLiteral("animator/brush/size"), width);
}

void OptionsBar::onSmoothingChanged(int tenths) {
    if (canvas_ != nullptr) {
        canvas_->setSmoothing(tenths / 10.0f);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("animator/brush/smoothing"),
                      tenths / 10.0);
}

void OptionsBar::onOpacityChanged(int percent) {
    if (canvas_ != nullptr) {
        canvas_->setStrokeOpacity(percent / 100.0f);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("animator/brush/opacity"), percent);
}

void OptionsBar::onStrokeColorPicked() {
    const QColor current =
        canvas_ != nullptr
            ? toQColor(canvas_->drawingOptions().strokeColor)
            : QColor(Qt::black);
    const QColor picked = QColorDialog::getColor(
        current, this, tr("Stroke color"),
        QColorDialog::ShowAlphaChannel);
    if (!picked.isValid()) {
        return;
    }
    if (canvas_ != nullptr) {
        canvas_->setStrokeColor(toAnimColor(picked));
    }
    strokeButton_->setIcon(colorIcon(picked));
    QSettings settings;
    settings.setValue(QStringLiteral("animator/strokeColor"), picked.name(QColor::HexArgb));
}

void OptionsBar::onFillColorPicked() {
    const QColor current =
        canvas_ != nullptr ? toQColor(canvas_->drawingOptions().fillColor)
                           : QColor(Qt::white);
    const QColor picked = QColorDialog::getColor(
        current, this, tr("Fill color"), QColorDialog::ShowAlphaChannel);
    if (!picked.isValid()) {
        return;
    }
    if (canvas_ != nullptr) {
        canvas_->setFillColor(toAnimColor(picked));
    }
    fillButton_->setIcon(colorIcon(picked));
    QSettings settings;
    settings.setValue(QStringLiteral("animator/fillColor"), picked.name(QColor::HexArgb));
}

void OptionsBar::onSwatchClicked() {
    auto* button = qobject_cast<QPushButton*>(sender());
    if (button == nullptr) {
        return;
    }
    const QColor swatch(button->property("swatch").toString());
    if (!swatch.isValid()) {
        return;
    }
    const bool toFill =
        QApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    if (toFill) {
        if (canvas_ != nullptr) {
            // Keep the picked alpha: a swatch replaces RGB only.
            icg::anim::AnimColor fill = canvas_->drawingOptions().fillColor;
            fill.r = swatch.red();
            fill.g = swatch.green();
            fill.b = swatch.blue();
            canvas_->setFillColor(fill);
        }
        fillButton_->setIcon(colorIcon(swatch));
        QSettings settings;
        settings.setValue(QStringLiteral("animator/fillColor"),
                          swatch.name(QColor::HexArgb));
    } else {
        if (canvas_ != nullptr) {
            icg::anim::AnimColor stroke = canvas_->drawingOptions().strokeColor;
            stroke.r = swatch.red();
            stroke.g = swatch.green();
            stroke.b = swatch.blue();
            canvas_->setStrokeColor(stroke);
        }
        strokeButton_->setIcon(colorIcon(swatch));
        QSettings settings;
        settings.setValue(QStringLiteral("animator/strokeColor"),
                          swatch.name(QColor::HexArgb));
    }
}
