// Incogine Studio - font preview tab (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Loads a project font file and previews a pangram + digits at an
// adjustable point size.
#pragma once

#include <QWidget>

class QLabel;
class QSpinBox;

class FontPreview : public QWidget {
    Q_OBJECT

public:
    explicit FontPreview(const QString& path, QWidget* parent = nullptr);

private slots:
    void onSizeChanged(int pointSize);

private:
    QLabel* preview_ = nullptr;
    QSpinBox* sizeBox_ = nullptr;
    QString family_;
};

