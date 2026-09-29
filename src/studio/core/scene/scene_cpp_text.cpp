// Incogine Studio — C++ scene text/bracket utilities (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// EOL/line splitting, whitespace, comment stripping, top-level
// splitting, number/literal/identifier helpers, and the
// comment/string-aware bracket matcher shared by the scene parser,
// font scan, and edit ops. Declared in scene_cpp_detail.h; the
// public API stays in scene_cpp.h.
#include "scene_cpp_detail.h"

#include <cctype>
#include <string>
#include <vector>

namespace icg {
namespace studio {
namespace scenecpp {

namespace detail {

std::string DetectEol(const std::string& text) {
    return text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
}

std::vector<std::string> SplitLines(const std::string& text, const std::string& eol) {
    std::vector<std::string> lines;
    lines.reserve(text.size() / 32 + 1);
    size_t start = 0;
    while (start <= text.size()) {
        size_t pos = text.find(eol, start);
        if (pos == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, pos - start));
        start = pos + eol.size();
    }
    // A trailing EOL produces one empty phantom line; drop it and let the
    // trailing-newline flag round-trip instead.
    if (!lines.empty() && lines.back().empty() &&
        (text.size() >= eol.size() &&
         text.compare(text.size() - eol.size(), eol.size(), eol) == 0)) {
        lines.pop_back();
    }
    return lines;
}

std::string JoinLines(const std::vector<std::string>& lines, const std::string& eol,
                      bool trailingNewline) {
    std::string out;
    size_t total = 0;
    for (const std::string& line : lines) {
        total += line.size() + eol.size();
    }
    out.reserve(total);
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += eol;
        }
        out += lines[i];
    }
    if (trailingNewline) {
        out += eol;
    }
    return out;
}

std::string Trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(b, e - b);
}

std::string LeadingWhitespace(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return s.substr(0, i);
}

std::string CollapseWhitespace(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool inSpace = true; // trim leading
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!inSpace) {
                out += ' ';
                inSpace = true;
            }
        } else {
            out += c;
            inSpace = false;
        }
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

// Splits on a delimiter at paren/bracket/brace depth 0, strings/comments
// intact (comments kept in the pieces for raw fidelity).
std::vector<std::string> SplitTopLevel(const std::string& s, char delim) {
    std::vector<std::string> parts;
    parts.reserve(8);
    std::string cur;
    cur.reserve(s.size());
    int pDepth = 0, bDepth = 0, cDepth = 0;
    bool inStr = false, inChar = false, inLine = false, inBlock = false;
    char strQuote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        const char n = i + 1 < s.size() ? s[i + 1] : 0;
        if (inLine) {
            cur += c;
            if (c == '\n') {
                inLine = false;
            }
            continue;
        }
        if (inBlock) {
            cur += c;
            if (c == '*' && n == '/') {
                cur += n;
                ++i;
                inBlock = false;
            }
            continue;
        }
        if (inStr || inChar) {
            cur += c;
            if (c == '\\' && n) {
                cur += n;
                ++i;
            } else if (c == strQuote) {
                inStr = inChar = false;
            }
            continue;
        }
        if (c == '/' && n == '/') {
            inLine = true;
            cur += c;
            continue;
        }
        if (c == '/' && n == '*') {
            inBlock = true;
            cur += c;
            continue;
        }
        if (c == '"' || c == '\'') {
            inStr = (c == '"');
            inChar = (c == '\'');
            strQuote = c;
            cur += c;
            continue;
        }
        if (c == '(') {
            ++pDepth;
        } else if (c == ')') {
            --pDepth;
        } else if (c == '[') {
            ++bDepth;
        } else if (c == ']') {
            --bDepth;
        } else if (c == '{') {
            ++cDepth;
        } else if (c == '}') {
            --cDepth;
        }
        if (c == delim && pDepth == 0 && bDepth == 0 && cDepth == 0) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);
    return parts;
}

// Comment-stripped copy for pattern matching (strings preserved).
std::string StripComments(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool inStr = false, inChar = false, inLine = false, inBlock = false;
    char quote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        const char n = i + 1 < s.size() ? s[i + 1] : 0;
        if (inLine) {
            if (c == '\n') {
                inLine = false;
                out += c;
            } else {
                out += ' ';
            }
            continue;
        }
        if (inBlock) {
            if (c == '*' && n == '/') {
                out += "  ";
                ++i;
                inBlock = false;
            } else if (c == '\n') {
                out += c;
            } else {
                out += ' ';
            }
            continue;
        }
        if (inStr || inChar) {
            out += c;
            if (c == '\\' && n) {
                out += n;
                ++i;
            } else if (c == quote) {
                inStr = inChar = false;
            }
            continue;
        }
        if (c == '/' && n == '/') {
            inLine = true;
            out += "  ";
            ++i;
            continue;
        }
        if (c == '/' && n == '*') {
            inBlock = true;
            out += "  ";
            ++i;
            continue;
        }
        if (c == '"' || c == '\'') {
            inStr = (c == '"');
            inChar = (c == '\'');
            quote = c;
        }
        out += c;
    }
    return out;
}

bool ParseDouble(const std::string& s, double& out) {
    const std::string t = Trim(s);
    if (t.empty()) {
        return false;
    }
    try {
        size_t pos = 0;
        out = std::stod(t, &pos);
        return pos == t.size();
    } catch (...) {
        return false;
    }
}

VecExpr ParseVec(const std::string& inner) {
    VecExpr vec;
    for (const std::string& part : SplitTopLevel(inner, ',')) {
        vec.parts.push_back(Trim(part));
    }
    vec.numeric = !vec.parts.empty();
    for (const std::string& part : vec.parts) {
        double v = 0;
        if (!ParseDouble(part, v)) {
            vec.numeric = false;
            break;
        }
        vec.values.push_back(v);
    }
    if (!vec.numeric) {
        vec.values.clear();
    }
    return vec;
}

std::string EscapeLiteral(const std::string& s, bool& ok) {
    ok = true;
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\r' || c == '\n') {
            ok = false;
            return {};
        }
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

bool IsIdentifier(const std::string& s) {
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) {
        return false;
    }
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    return true;
}

// ---------- offset <-> line mapping ----------

std::vector<size_t> LineStarts(const std::string& text) {
    std::vector<size_t> starts = {0};
    starts.reserve(text.size() / 32 + 1);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            starts.push_back(i + 1);
        }
    }
    return starts;
}

size_t OffsetToLine(const std::vector<size_t>& starts, size_t off) {
    // 1-based line containing offset off.
    size_t lo = 0, hi = starts.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (starts[mid] <= off) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo; // count of starts <= off == 1-based line
}

// Finds `close` matching the opener at `open` (which must be ( [ or {),
// strings/comments/chars skipped. Returns npos when unbalanced.
size_t MatchBracket(const std::string& text, size_t open, char close) {
    // Explicit stack (no fixed cap) for the openers seen since `open`.
    std::string stack;
    stack.reserve(32);
    bool inStr = false, inChar = false, inLine = false, inBlock = false;
    char quote = 0;
    for (size_t i = open; i < text.size(); ++i) {
        const char c = text[i];
        const char n = i + 1 < text.size() ? text[i + 1] : 0;
        if (inLine) {
            if (c == '\n') {
                inLine = false;
            }
            continue;
        }
        if (inBlock) {
            if (c == '*' && n == '/') {
                ++i;
                inBlock = false;
            }
            continue;
        }
        if (inStr || inChar) {
            if (c == '\\' && n) {
                ++i;
            } else if (c == quote) {
                inStr = inChar = false;
            }
            continue;
        }
        if (c == '/' && n == '/') {
            inLine = true;
            ++i;
            continue;
        }
        if (c == '/' && n == '*') {
            inBlock = true;
            ++i;
            continue;
        }
        if (c == '"' || c == '\'') {
            inStr = (c == '"');
            inChar = (c == '\'');
            quote = c;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') {
            stack.push_back(c);
            continue;
        }
        if (c == ')' || c == ']' || c == '}') {
            if (stack.empty()) {
                return std::string::npos;
            }
            const char o = stack.back();
            stack.pop_back();
            const bool pairs = (o == '(' && c == ')') || (o == '[' && c == ']') ||
                               (o == '{' && c == '}');
            if (!pairs) {
                return std::string::npos;
            }
            if (stack.empty()) {
                if (i <= open) {
                    return std::string::npos;
                }
                return (c == close) ? i : std::string::npos;
            }
        }
    }
    return std::string::npos;
}

} // namespace detail

} // namespace scenecpp
} // namespace studio
} // namespace icg
