// Incogine Studio — Studio-wide preferences dialog.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Modal QSettings-backed editor preferences (viewport zoom invert today,
// room to grow). Zero coupling: reads/writes QSettings directly, so any
// caller is one line.
#pragma once

#include <QDialog>

class QWidget;

class StudioSettingsDialog : public QDialog {
    Q_OBJECT

public:
    // Opens modally; persists on OK, discards on Cancel.
    static void open(QWidget* parent);

private:
    explicit StudioSettingsDialog(QWidget* parent);
};
