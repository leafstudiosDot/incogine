// Incogine Studio — find-in-files panel (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Recursive text search over the project source tree. Build output,
// vendored third-party sources, and binaries are skipped.
#pragma once

#include <QWidget>

#include <string>

class QCheckBox;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class SearchPanel : public QWidget {
    Q_OBJECT

public:
    explicit SearchPanel(const std::string& projectRoot, QWidget* parent = nullptr);

signals:
    void openFile(const QString& path, int line);

private slots:
    void runSearch();
    void onResultActivated(QTreeWidgetItem* item);

private:
    static bool isSkippedDir(const QString& name);
    static bool isBinarySuffix(const QString& suffix);

    std::string projectRoot_;
    QLineEdit* queryField_ = nullptr;
    QCheckBox* caseBox_ = nullptr;
    QTreeWidget* results_ = nullptr;
};
