// Incogine Studio - Studio-wide preferences dialog implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "settings_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QSettings>
#include <QVBoxLayout>

StudioSettingsDialog::StudioSettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Studio Settings"));
    auto* layout = new QVBoxLayout(this);
    auto* zoomInvert = new QCheckBox(tr("Invert mouse wheel zoom (Scene tab)"));
    zoomInvert->setObjectName("zoomInvert");
    zoomInvert->setToolTip(tr("Scroll down zooms in instead of up"));
    layout->addWidget(zoomInvert);
    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void StudioSettingsDialog::open(QWidget* parent) {
    StudioSettingsDialog dialog(parent);
    QSettings settings;
    auto* zoomInvert =
        dialog.findChild<QCheckBox*>(QStringLiteral("zoomInvert"));
    if (zoomInvert) {
        zoomInvert->setChecked(
            settings.value(QStringLiteral("scene/zoomInvert"), false).toBool());
    }
    if (dialog.exec() == QDialog::Accepted && zoomInvert) {
        settings.setValue(QStringLiteral("scene/zoomInvert"),
                          zoomInvert->isChecked());
    }
}

