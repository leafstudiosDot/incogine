// Incogine Studio - code editor implementation (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "code_editor.h"

#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QTextBlock>
#include <QVBoxLayout>

namespace {

// Hardcoded highlight colors are unreadable when the OS/app runs dark, so
// the palette is chosen from the application theme (VS Code inspired).
bool isDarkTheme() {
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

struct ThemeColors {
    QColor keyword;
    QColor type;
    QColor string;
    QColor comment;
    QColor number;
    QColor preproc;
    QColor function;
};

ThemeColors currentTheme() {
    if (isDarkTheme()) {
        return {QColor("#569CD6"), QColor("#4EC9B0"), QColor("#CE9178"),
                QColor("#6A9955"), QColor("#B5CEA8"), QColor("#C586C0"),
                QColor("#DCDCAA")};
    }
    return {Qt::darkBlue, Qt::darkMagenta, Qt::darkRed, Qt::darkGreen,
            QColor(0, 128, 128), QColor(128, 0, 128), QColor(0, 0, 160)};
}

} // namespace

// ---- StudioHighlighter ----

StudioHighlighter::StudioHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document) {
    setLanguage(Language::Plain);
}

QTextCharFormat StudioHighlighter::makeFormat(const QColor& color, bool bold, bool italic) {
    QTextCharFormat format;
    format.setForeground(color);
    if (bold) {
        format.setFontWeight(QFont::Bold);
    }
    if (italic) {
        format.setFontItalic(true);
    }
    return format;
}

QString StudioHighlighter::keywordPattern(const std::vector<const char*>& words) {
    QString pattern = "\\b(";
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) {
            pattern += "|";
        }
        pattern += words[i];
    }
    pattern += ")\\b";
    return pattern;
}

void StudioHighlighter::addRule(const QString& pattern, const QTextCharFormat& format) {
    rules_.push_back({pattern, format});
}

StudioHighlighter::Language StudioHighlighter::languageForPath(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "cpp" || suffix == "h" || suffix == "hpp" || suffix == "c" || suffix == "cc") {
        return Language::Cpp;
    }
    if (suffix == "cs") {
        return Language::CSharp;
    }
    if (suffix == "kodo") {
        return Language::Kodo;
    }
    if (suffix == "py") {
        return Language::Python;
    }
    if (suffix == "xml" || suffix == "csproj" || suffix == "vcxproj" || suffix == "props" ||
        suffix == "targets" || suffix == "svg") {
        return Language::Xml;
    }
    if (suffix == "json") {
        return Language::Json;
    }
    return Language::Plain;
}

void StudioHighlighter::setLanguage(Language language) {
    language_ = language;
    rules_.clear();
    commentStart_.clear();
    commentEnd_.clear();
    const ThemeColors theme = currentTheme();
    commentFormat_ = makeFormat(theme.comment);
    commentFormat_.setFontItalic(true);

    const QTextCharFormat keyword = makeFormat(theme.keyword, true);
    const QTextCharFormat type = makeFormat(theme.type);
    const QTextCharFormat string = makeFormat(theme.string);
    const QTextCharFormat number = makeFormat(theme.number);
    const QTextCharFormat preproc = makeFormat(theme.preproc);
    const QTextCharFormat function = makeFormat(theme.function);

    switch (language_) {
        case Language::Cpp:
            addRule(keywordPattern({"alignas", "alignof", "and", "and_eq", "asm", "auto",
                                    "bitand", "bitor", "bool", "break", "case", "catch",
                                    "char", "char8_t", "char16_t", "char32_t", "class",
                                    "compl", "concept", "const", "consteval", "constexpr",
                                    "constinit", "const_cast", "continue", "co_await",
                                    "co_return", "co_yield", "decltype", "default", "delete",
                                    "do", "double", "dynamic_cast", "else", "enum",
                                    "explicit", "export", "extern", "false", "float",
                                    "for", "friend", "goto", "if", "inline", "int",
                                    "long", "mutable", "namespace", "new", "noexcept",
                                    "not", "not_eq", "nullptr", "operator", "or",
                                    "or_eq", "private", "protected", "public", "register",
                                    "reinterpret_cast", "requires", "return", "short",
                                    "signed", "sizeof", "static", "static_assert",
                                    "static_cast", "struct", "switch", "template", "this",
                                    "thread_local", "throw", "true", "try", "typedef",
                                    "typeid", "typename", "union", "unsigned", "using",
                                    "virtual", "void", "volatile", "wchar_t", "while",
                                    "xor", "xor_eq"}),
                    keyword);
            addRule("\\b(Q[A-Z]\\w*|std::\\w+|[A-Z]\\w*)\\b", type);
            addRule("\"(\\\\.|[^\"\\\\])*\"", string);
            addRule("'(\\\\.|[^'\\\\])'", string);
            addRule("//[^\n]*", commentFormat_);
            addRule("#\\s*\\w+.*", preproc);
            addRule("\\b\\d[\\dxX'\\.abcdefABCDEFulUL]*\\b", number);
            addRule("\\b[A-Za-z_]\\w*(?=\\()", function);
            commentStart_ = "/*";
            commentEnd_ = "*/";
            break;
        case Language::CSharp:
            addRule(keywordPattern({"abstract", "as", "base", "bool", "break", "byte",
                                    "case", "catch", "char", "checked", "class", "const",
                                    "continue", "decimal", "default", "delegate", "do",
                                    "double", "else", "enum", "event", "explicit",
                                    "extern", "false", "finally", "fixed", "float",
                                    "for", "foreach", "get", "goto", "if", "implicit",
                                    "in", "int", "interface", "internal", "is", "lock",
                                    "long", "namespace", "new", "null", "object",
                                    "operator", "out", "override", "params", "partial",
                                    "private", "protected", "public", "readonly", "record",
                                    "ref", "required", "return", "sbyte", "sealed",
                                    "set", "short", "sizeof", "stackalloc", "static",
                                    "string", "struct", "switch", "this", "throw",
                                    "true", "try", "typeof", "uint", "ulong", "unchecked",
                                    "unsafe", "ushort", "using", "value", "var",
                                    "virtual", "void", "volatile", "while"}),
                    keyword);
            addRule("\\b[A-Z]\\w*\\b", type);
            addRule("@?\"(\\\\.|[^\"\\\\])*\"", string);
            addRule("'(\\\\.|[^'\\\\])'", string);
            addRule("//[^\n]*", commentFormat_);
            addRule("#\\s*\\w+.*", preproc);
            addRule("\\b\\d[\\d'\\.abcdefABCDEFulUL]*\\b", number);
            addRule("\\b[A-Za-z_]\\w*(?=\\()", function);
            commentStart_ = "/*";
            commentEnd_ = "*/";
            break;
        case Language::Kodo:
            // Keywords mirror Lexer::keyword() in kodolexer.cpp (`in` is
            // contextual, not reserved, so it stays unhighlighted).
            addRule(keywordPattern({"var", "function", "return", "if", "else",
                                    "while", "for", "foreach", "switch", "case",
                                    "default", "break", "continue", "try", "catch",
                                    "import", "from", "as", "true", "false", "null",
                                    "this"}),
                    keyword);
            addRule("\\b(Object|Scene|Engine|Audio|Sprite|Vector|Color|Position|"
                    "Scale|Rotation|Transform|Save|Time)\\b",
                    type);
            addRule("\"(\\\\.|[^\"\\\\])*\"", string);
            addRule("//[^\n]*", commentFormat_);
            addRule("\\b\\d[\\d'\\.]*\\b", number);
            addRule("\\b[A-Za-z_]\\w*(?=\\()", function);
            commentStart_ = "/*";
            commentEnd_ = "*/";
            break;
        case Language::Python:
            addRule(keywordPattern({"False", "None", "True", "and", "as",
                                    "assert", "async", "await", "break", "class",
                                    "continue", "def", "del", "elif", "else",
                                    "except", "finally", "for", "from", "global",
                                    "if", "import", "in", "is", "lambda", "nonlocal",
                                    "not", "or", "pass", "raise", "return", "try",
                                    "while", "with", "yield"}),
                    keyword);
            addRule("\"\"\"|'''|\"|'", string);
            addRule("#[^\n]*", commentFormat_);
            addRule("\\b\\d[\\d'\\.]*\\b", number);
            addRule("\\b[A-Za-z_]\\w*(?=\\()", function);
            addRule("@[A-Za-z_]\\w*", preproc);
            break;
        case Language::Xml:
            addRule("<!--[^\n]*", commentFormat_);
            addRule("\"[^\"]*\"", string);
            addRule("'<[^>]*>'|'[^']*'", string);
            addRule("</?\\??[\\w:.-]+|\\??/?>", keyword);
            addRule("[\\w:.-]+(?==)", type);
            commentStart_ = "<!--";
            commentEnd_ = "-->";
            break;
        case Language::Json:
            addRule("\"([^\"\\\\]|\\\\.)*\"(?=\\s*:)", keyword);
            addRule("\"([^\"\\\\]|\\\\.)*\"", string);
            addRule("\\b(true|false|null)\\b", type);
            addRule("-?\\b\\d[\\d\\.eE+-]*\\b", number);
            break;
        case Language::Plain:
            break;
    }
    rehighlight();
}

void StudioHighlighter::refresh() {
    setLanguage(language_);
}

void StudioHighlighter::highlightBlock(const QString& text) {
    for (const Rule& rule : rules_) {
        QRegularExpression expression(rule.pattern);
        QRegularExpressionMatchIterator it = expression.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Multi-line constructs (C-style comments, XML comments).
    if (commentStart_.isEmpty()) {
        return;
    }
    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf(commentStart_);
    }
    while (startIndex >= 0) {
        const int endIndex = text.indexOf(commentEnd_, startIndex + commentStart_.length());
        int length;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            length = text.length() - startIndex;
        } else {
            length = endIndex - startIndex + commentEnd_.length();
        }
        setFormat(startIndex, length, commentFormat_);
        startIndex = text.indexOf(commentStart_, startIndex + length);
    }
}

// ---- CodeEditor ----

CodeEditor::CodeEditor(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    editor_ = new QPlainTextEdit();
    editor_->setPlaceholderText(tr("Open a file from the Project dock (double-click)."));
    QFont font("Consolas", 10);
    font.setStyleHint(QFont::Monospace);
    editor_->setFont(font);
    layout->addWidget(editor_);

    highlighter_ = new StudioHighlighter(editor_->document());

    findBar_ = new QWidget();
    QHBoxLayout* bar = new QHBoxLayout(findBar_);
    bar->setContentsMargins(0, 0, 0, 0);
    findField_ = new QLineEdit();
    findField_->setPlaceholderText(tr("Find"));
    findField_->setClearButtonEnabled(true);
    replaceField_ = new QLineEdit();
    replaceField_->setPlaceholderText(tr("Replace"));
    replaceField_->setClearButtonEnabled(true);
    QPushButton* next = new QPushButton(tr("Next"));
    QPushButton* prev = new QPushButton(tr("Prev"));
    QPushButton* replaceOne = new QPushButton(tr("Replace"));
    QPushButton* replaceAll = new QPushButton(tr("All"));
    bar->addWidget(new QLabel(tr("Find:")));
    bar->addWidget(findField_);
    bar->addWidget(new QLabel(tr("Replace:")));
    bar->addWidget(replaceField_);
    bar->addWidget(prev);
    bar->addWidget(next);
    bar->addWidget(replaceOne);
    bar->addWidget(replaceAll);
    layout->addWidget(findBar_);
    findBar_->setVisible(false);

    connect(next, &QPushButton::clicked, this, &CodeEditor::findNext);
    connect(prev, &QPushButton::clicked, this, &CodeEditor::findPrev);
    connect(replaceOne, &QPushButton::clicked, this, &CodeEditor::replaceOne);
    connect(replaceAll, &QPushButton::clicked, this, &CodeEditor::replaceAll);
    connect(findField_, &QLineEdit::returnPressed, this, &CodeEditor::findNext);

    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this] { openFind(false); });
    auto* replaceShortcut = new QShortcut(QKeySequence::Replace, this);
    connect(replaceShortcut, &QShortcut::activated, this, [this] { openFind(true); });

    // Rebuild highlight colors when the OS/app theme changes (light/dark).
    connect(qApp, &QGuiApplication::paletteChanged, this, &CodeEditor::refreshHighlight);
}

void CodeEditor::refreshHighlight() {
    highlighter_->refresh();
}

void CodeEditor::setText(const QString& text) {
    editor_->setPlainText(text);
}

QString CodeEditor::text() const {
    return editor_->toPlainText();
}

void CodeEditor::setLanguageFromPath(const QString& path) {
    highlighter_->setLanguage(StudioHighlighter::languageForPath(path));
}

void CodeEditor::openFind(bool withReplace) {
    findBar_->setVisible(true);
    replaceField_->setVisible(withReplace);
    findField_->setFocus();
    findField_->selectAll();
    if (editor_->textCursor().hasSelection()) {
        findField_->setText(editor_->textCursor().selectedText());
    }
}

void CodeEditor::gotoLine(int line) {
    if (line < 1) {
        return;
    }
    QTextCursor cursor(editor_->document()->findBlockByLineNumber(line - 1));
    editor_->setTextCursor(cursor);
    editor_->centerCursor();
}

bool CodeEditor::findOnce(QTextDocument::FindFlags flags) {
    const QString needle = findField_->text();
    if (needle.isEmpty()) {
        return false;
    }
    QTextCursor found = editor_->document()->find(needle, editor_->textCursor(), flags);
    if (found.isNull()) {
        // Wrap around.
        QTextCursor restart(editor_->document());
        if (flags & QTextDocument::FindBackward) {
            restart.movePosition(QTextCursor::End);
        }
        found = editor_->document()->find(needle, restart, flags);
    }
    if (!found.isNull()) {
        editor_->setTextCursor(found);
        return true;
    }
    return false;
}

void CodeEditor::findNext() {
    findOnce(QTextDocument::FindFlags());
}

void CodeEditor::findPrev() {
    findOnce(QTextDocument::FindBackward);
}

void CodeEditor::replaceOne() {
    QTextCursor cursor = editor_->textCursor();
    if (cursor.hasSelection() && cursor.selectedText() == findField_->text()) {
        cursor.insertText(replaceField_->text());
    }
    findNext();
}

void CodeEditor::replaceAll() {
    const QString needle = findField_->text();
    if (needle.isEmpty()) {
        return;
    }
    // Collect matches first, then replace back-to-front so offsets stay
    // valid even when the replacement contains the needle.
    QList<QTextCursor> matches;
    QTextCursor cursor(editor_->document());
    for (;;) {
        cursor = editor_->document()->find(needle, cursor);
        if (cursor.isNull()) {
            break;
        }
        matches.append(cursor);
        if (matches.size() > 100000) {
            break;
        }
    }
    QTextCursor edit(editor_->document());
    edit.beginEditBlock();
    for (int i = matches.size() - 1; i >= 0; --i) {
        editor_->setTextCursor(matches[i]);
        editor_->textCursor().insertText(replaceField_->text());
    }
    edit.endEditBlock();
}

