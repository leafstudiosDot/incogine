// Incogine Studio — code editor widget (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Plain-text editor with per-language syntax highlighting (C/C++, C#,
// Kodo, Python, XML, JSON) and an inline find/replace bar. Kodo keywords
// come from the engine lexer (`src/core/scripting/kodo/kodolexer.cpp`).
#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QWidget>

#include <vector>

class QLineEdit;
class QPlainTextEdit;

class StudioHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    enum class Language { Plain, Cpp, CSharp, Kodo, Python, Xml, Json };

    explicit StudioHighlighter(QTextDocument* document);
    void setLanguage(Language language);
    void refresh(); // rebuild rules (e.g. after a theme change)
    Language language() const { return language_; }

    static Language languageForPath(const QString& path);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QString pattern;
        QTextCharFormat format;
    };

    void addRule(const QString& pattern, const QTextCharFormat& format);
    static QTextCharFormat makeFormat(const QColor& color, bool bold = false,
                                      bool italic = false);
    static QString keywordPattern(const std::vector<const char*>& words);

    Language language_ = Language::Plain;
    std::vector<Rule> rules_;
    QTextCharFormat commentFormat_;
    QString commentStart_;
    QString commentEnd_;
};

class CodeEditor : public QWidget {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);

    void setText(const QString& text);
    QString text() const;
    void setLanguageFromPath(const QString& path);
    void openFind(bool withReplace);
    void gotoLine(int line); // 1-based; -1 keeps position
    QPlainTextEdit* editor() const { return editor_; }

private slots:
    void refreshHighlight();

private slots:
    void findNext();
    void findPrev();
    void replaceOne();
    void replaceAll();

private:
    bool findOnce(QTextDocument::FindFlags flags);

    QPlainTextEdit* editor_;
    StudioHighlighter* highlighter_;
    QWidget* findBar_;
    QLineEdit* findField_;
    QLineEdit* replaceField_;
};
