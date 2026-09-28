// Incogine Studio — find-in-files implementation (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "search_panel.h"

#include <QCheckBox>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int kMaxResults = 2000;
constexpr int kMaxFileBytes = 2 * 1024 * 1024; // skip huge files

} // namespace

SearchPanel::SearchPanel(const std::string& projectRoot, QWidget* parent)
    : QWidget(parent), projectRoot_(projectRoot) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* bar = new QHBoxLayout();
    queryField_ = new QLineEdit();
    queryField_->setPlaceholderText(tr("Search project files..."));
    queryField_->setClearButtonEnabled(true);
    caseBox_ = new QCheckBox(tr("Case sensitive"));
    QPushButton* go = new QPushButton(tr("Search"));
    bar->addWidget(new QLabel(tr("Find in files:")));
    bar->addWidget(queryField_, 1);
    bar->addWidget(caseBox_);
    bar->addWidget(go);
    layout->addLayout(bar);

    results_ = new QTreeWidget();
    results_->setHeaderLabels({tr("File"), tr("Line"), tr("Text")});
    results_->setColumnWidth(0, 320);
    results_->setColumnWidth(1, 60);
    layout->addWidget(results_);

    connect(go, &QPushButton::clicked, this, &SearchPanel::runSearch);
    connect(queryField_, &QLineEdit::returnPressed, this, &SearchPanel::runSearch);
    connect(results_, &QTreeWidget::itemDoubleClicked, this, &SearchPanel::onResultActivated);
}

bool SearchPanel::isSkippedDir(const QString& name) {
    // Build outputs, VCS metadata, vendored/upstream sources, and IDE state.
    // `src/` itself is searched; `src/scripts/csharp/*/obj|bin` fall under
    // the generic bin/obj rules via full-path checks in runSearch().
    return name == ".git" || name == ".vs" || name == "reqs" || name == "emsdk" ||
           name == "out" || name == "certs" || name == "build" ||
           name.startsWith("build-") || name == "Debug" || name == "Release" ||
           name == "GeneratedFiles";
}

bool SearchPanel::isBinarySuffix(const QString& suffix) {
    return suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "gif" ||
           suffix == "bmp" || suffix == "tiff" || suffix == "webp" || suffix == "ico" ||
           suffix == "icns" || suffix == "ttf" || suffix == "otf" || suffix == "ogg" ||
           suffix == "wav" || suffix == "mp3" || suffix == "flac" || suffix == "dll" ||
           suffix == "exe" || suffix == "lib" || suffix == "obj" || suffix == "pdb" ||
           suffix == "res" || suffix == "ilk" || suffix == "exp" || suffix == "dat" ||
           suffix == "data" || suffix == "wasm" || suffix == "zip" || suffix == "incoba" ||
           suffix == "incobai" || suffix == "nupkg";
}

void SearchPanel::runSearch() {
    results_->clear();
    const QString needle = queryField_->text();
    if (needle.isEmpty()) {
        return;
    }
    const Qt::CaseSensitivity sensitivity =
        caseBox_->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QString root = QString::fromStdString(projectRoot_);

    QDirIterator it(root,
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    int hits = 0;
    while (it.hasNext() && hits < kMaxResults) {
        it.next();
        const QFileInfo info = it.fileInfo();
        // Prune skipped directories (iterator still descends, but entries
        // under them are filtered by path segments).
        bool skipped = false;
        QString relDir = root.isEmpty() ? QString() : info.dir().absolutePath().mid(root.length());
        for (const QString& segment : relDir.split('/', Qt::SkipEmptyParts)) {
            if (isSkippedDir(segment) || segment == "obj" || segment == "bin") {
                skipped = true;
                break;
            }
        }
        if (skipped) {
            continue;
        }
        if (isBinarySuffix(info.suffix().toLower())) {
            continue;
        }
        if (info.size() > kMaxFileBytes) {
            continue;
        }
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        // NUL sniff: treat as binary.
        const QByteArray head = file.peek(8192);
        if (head.contains('\0')) {
            continue;
        }
        QTextStream in(&file);
        int line = 0;
        while (!in.atEnd() && hits < kMaxResults) {
            const QString text = in.readLine();
            ++line;
            if (text.contains(needle, sensitivity)) {
                auto* item = new QTreeWidgetItem(
                    results_, {info.absoluteFilePath(), QString::number(line), text.trimmed()});
                item->setData(0, Qt::UserRole, info.absoluteFilePath());
                item->setData(1, Qt::UserRole, line);
                ++hits;
            }
        }
    }
    results_->setHeaderLabels(
        {tr("File"), tr("Line"), tr("Text — %1 hit(s)").arg(hits)});
}

void SearchPanel::onResultActivated(QTreeWidgetItem* item) {
    if (!item) {
        return;
    }
    emit openFile(item->data(0, Qt::UserRole).toString(),
                  item->data(1, Qt::UserRole).toInt());
}
