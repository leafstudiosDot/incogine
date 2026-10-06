// Incogine Studio - font preview implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "font_preview.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

constexpr const char* kPangram =
    "The quick brown fox jumps over a lazy dog... 1 2 3 4 5 6 7 8 9 0";

} // namespace

FontPreview::FontPreview(const QString& path, QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(path));
    top->addStretch(1);
    top->addWidget(new QLabel(tr("Preview size:")));
    sizeBox_ = new QSpinBox();
    sizeBox_->setRange(6, 144);
    sizeBox_->setValue(28);
    sizeBox_->setSuffix(tr(" pt"));
    top->addWidget(sizeBox_);
    layout->addLayout(top);

    preview_ = new QLabel();
    preview_->setWordWrap(true);
    preview_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    preview_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    const int fontId = QFontDatabase::addApplicationFont(path);
    if (fontId == -1) {
        preview_->setText(tr("Could not load font: %1").arg(path));
    } else {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (families.isEmpty()) {
            preview_->setText(tr("Font loaded but has no families: %1").arg(path));
        } else {
            family_ = families.first();
            preview_->setText(kPangram);
            onSizeChanged(sizeBox_->value());
        }
    }

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setWidget(preview_);
    layout->addWidget(scroll, 1);

    connect(sizeBox_, &QSpinBox::valueChanged, this, &FontPreview::onSizeChanged);
}

void FontPreview::onSizeChanged(int pointSize) {
    if (family_.isEmpty()) {
        return;
    }
    QFont font(family_, pointSize);
    preview_->setFont(font);
}

