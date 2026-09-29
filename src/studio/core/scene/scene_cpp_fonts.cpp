// Incogine Studio — C++ scene font/text-label scan (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Header `Font` declarations plus file-wide setFontFile/setTextContent/
// setColor/renderUI call sites, and the guarded SetTextPosition rewrite.
// Shared helpers: scene_cpp_detail.h. Public API: scene_cpp.h.
#include "scene_cpp.h"
#include "scene_cpp_detail.h"

#include <cctype>
#include <regex>
#include <unordered_map>

namespace icg {
namespace studio {
namespace scenecpp {
namespace {

int ConstIndex(const std::string& bracketInner, bool& isConst) {
    const std::string t = detail::Trim(bracketInner);
    isConst = !t.empty();
    for (char c : t) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            isConst = false;
            break;
        }
    }
    if (!isConst) {
        return -1;
    }
    try {
        return std::stoi(t);
    } catch (...) {
        isConst = false;
        return -1;
    }
}

// Resolves an array size expression from header constants: plain literals,
// `static constexpr int N = 42;`, and the dominant codebase idiom
// `static constexpr int N = sizeof(Arr) / sizeof(Arr[0]);` where Arr has a
// visible braced initializer (counted). Returns 0 when unresolvable.
int ResolveArraySize(const std::string& headerText, const std::string& expr) {
    std::string t = detail::Trim(expr);
    if (!t.empty()) {
        bool digits = true;
        for (char c : t) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                digits = false;
                break;
            }
        }
        if (digits) {
            try {
                const int n = std::stoi(t);
                return (n >= 1 && n <= 4096) ? n : 0;
            } catch (...) {
                return 0;
            }
        }
    }
    // constexpr lookup table: raw value strings (digits or the
    // sizeof idiom), resolved below.
    std::unordered_map<std::string, std::string> constexprValues;
    std::unordered_map<std::string, int> arrayCounts;
    {
        static const std::regex constexprRe(
            "constexpr\\s+(?:const\\s+)?[A-Za-z_][\\w\\s\\*:]*?\\b([A-Za-z_]\\w*)\\s*="
            "\\s*([^;]+);");
        std::string::const_iterator it = headerText.begin();
        std::smatch m;
        while (std::regex_search(it, headerText.cend(), m, constexprRe)) {
            constexprValues[m[1].str()] = detail::Trim(m[2].str());
            it = m.suffix().first;
            if (it == headerText.cend()) {
                break;
            }
        }
    }
    {
        // `Type name[] = { ... };` — count top-level initializers.
        static const std::regex arrayInitRe(
            "([A-Za-z_]\\w*)\\s*\\[\\s*\\]\\s*=\\s*\\{");
        std::string::const_iterator it = headerText.begin();
        std::smatch m;
        while (std::regex_search(it, headerText.cend(), m, arrayInitRe)) {
            const size_t matchOff =
                static_cast<size_t>(m.position(0) + (it - headerText.begin()));
            const size_t openOff = headerText.find('{', matchOff);
            if (openOff != std::string::npos) {
                const size_t closeOff = detail::MatchBracket(headerText, openOff, '}');
                if (closeOff != std::string::npos && closeOff > openOff + 1) {
                    const std::vector<std::string> parts = detail::SplitTopLevel(
                        headerText.substr(openOff + 1, closeOff - openOff - 1), ',');
                    int count = 0;
                    for (const std::string& part : parts) {
                        if (!detail::Trim(part).empty()) {
                            ++count;
                        }
                    }
                    if (count >= 1 && count <= 4096) {
                        arrayCounts[m[1].str()] = count;
                    }
                }
            }
            it = m.suffix().first;
            if (it == headerText.cend()) {
                break;
            }
        }
    }
    const auto ci = constexprValues.find(t);
    if (ci == constexprValues.end()) {
        return 0;
    }
    const std::string& value = ci->second;
    bool digits = !value.empty();
    for (char c : value) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            digits = false;
            break;
        }
    }
    if (digits) {
        try {
            const int n = std::stoi(value);
            return (n >= 1 && n <= 4096) ? n : 0;
        } catch (...) {
            return 0;
        }
    }
    static const std::regex sizeofRe(
        "sizeof\\s*\\(\\s*([A-Za-z_]\\w*)\\s*\\)\\s*/\\s*sizeof\\s*\\(\\s*\\1\\s*\\[0\\]\\s*\\)");
    std::smatch m;
    if (std::regex_match(value, m, sizeofRe)) {
        const auto ai = arrayCounts.find(m[1].str());
        if (ai != arrayCounts.end()) {
            return ai->second;
        }
    }
    return 0;
}

} // namespace

namespace detail {

void ParseFonts(const std::string& headerText, const std::string& sourceText,
                SceneModel& model) {
    const std::vector<size_t> srcStarts = LineStarts(sourceText);
    std::unordered_map<std::string, size_t> fontIndex;

    // 1. Member declarations from the header (literal sizes plus the
    //    codebase's constexpr/sizeof idioms; anything else stays unknown).
    {
        static const std::regex declRe(
            "^\\s*Font\\s+([A-Za-z_]\\w*)\\s*(?:\\[\\s*([^\\]]+)\\s*\\])?\\s*;");
        size_t start = 0;
        while (start <= headerText.size()) {
            size_t eol = headerText.find('\n', start);
            if (eol == std::string::npos) {
                eol = headerText.size();
            }
            std::string line = headerText.substr(start, eol - start);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            std::smatch m;
            if (std::regex_match(line, m, declRe)) {
                FontDecl decl;
                decl.varName = m[1].str();
                if (m[2].matched) {
                    const int n = ResolveArraySize(headerText, m[2].str());
                    if (n <= 0) {
                        // Unresolvable size: skip (documented limitation).
                        if (eol == headerText.size()) {
                            break;
                        }
                        start = eol + 1;
                        continue;
                    }
                    decl.isArray = true;
                    decl.arraySize = n;
                }
                fontIndex[decl.varName] = model.fonts.size();
                model.fonts.push_back(decl);
                const int n = decl.isArray ? decl.arraySize : 1;
                for (int i = 0; i < n; ++i) {
                    TextItem item;
                    item.varName = decl.varName;
                    item.index = decl.isArray ? i : -1;
                    model.texts.push_back(item);
                }
            }
            if (eol == headerText.size()) {
                break;
            }
            start = eol + 1;
        }
    }
    if (model.fonts.empty()) {
        return;
    }

    auto itemsOf = [&](const std::string& var) {
        std::vector<size_t> out;
        for (size_t i = 0; i < model.texts.size(); ++i) {
            if (model.texts[i].varName == var) {
                out.push_back(i);
            }
        }
        return out;
    };
    // 2. setFontFile("path", pts) — per font (index ignored, uniform).
    {
        static const std::regex fontFileRe(
            "([A-Za-z_]\\w*)\\s*(\\[[^\\]]*\\])?\\s*\\.\\s*setFontFile\\s*\\(\\s*"
            "\"((?:[^\"\\\\]|\\\\.)*)\"\\s*,\\s*([0-9]+)\\s*\\)");
        std::string::const_iterator it = sourceText.begin();
        std::smatch m;
        while (std::regex_search(it, sourceText.cend(), m, fontFileRe)) {
            const auto fit = fontIndex.find(m[1].str());
            if (fit != fontIndex.end()) {
                FontDecl& decl = model.fonts[fit->second];
                decl.file = m[3].str();
                decl.pointSize = std::stoi(m[4].str());
                decl.hasFile = true;
            }
            it = m.suffix().first;
        }
    }
    // 3. Literal setTextContent("...") — last wins per item.
    {
        static const std::regex textRe(
            "([A-Za-z_]\\w*)\\s*(\\[[^\\]]*\\])?\\s*\\.\\s*setTextContent\\s*\\(\\s*"
            "(?:u8)?\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\)");
        std::string::const_iterator it = sourceText.begin();
        std::smatch m;
        while (std::regex_search(it, sourceText.cend(), m, textRe)) {
            const std::string var = m[1].str();
            bool isConst = false;
            int idx = -1;
            if (m[2].matched) {
                idx = ConstIndex(m[2].str().substr(1, m[2].str().size() - 2), isConst);
            }
            for (size_t ti : itemsOf(var)) {
                TextItem& item = model.texts[ti];
                if (isConst && item.index != idx) {
                    continue;
                }
                item.content = m[3].str();
                item.hasContent = true;
            }
            it = m.suffix().first;
        }
    }
    // 4. Numeric setColor(r, g, b, a) — last wins per item.
    {
        static const std::regex colorCallRe(
            "([A-Za-z_]\\w*)\\s*(\\[[^\\]]*\\])?\\s*\\.\\s*setColor\\s*\\(");
        std::string::const_iterator it = sourceText.begin();
        std::smatch m;
        while (std::regex_search(it, sourceText.cend(), m, colorCallRe)) {
            const size_t callOff =
                static_cast<size_t>(m.position(0) + (it - sourceText.begin()));
            const size_t openOff = sourceText.find('(', callOff);
            size_t adv = (openOff == std::string::npos)
                             ? sourceText.size()
                             : openOff + 1;
            if (openOff != std::string::npos) {
                const size_t closeOff = MatchBracket(sourceText, openOff, ')');
                if (closeOff != std::string::npos) {
                    const std::string inner =
                        sourceText.substr(openOff + 1, closeOff - openOff - 1);
                    const std::vector<std::string> parts = SplitTopLevel(inner, ',');
                    if (parts.size() == 4) {
                        int rgba[4];
                        bool ok = true;
                        for (int k = 0; k < 4 && ok; ++k) {
                            double v = 0;
                            ok = ParseDouble(parts[k], v);
                            rgba[k] = static_cast<int>(v);
                        }
                        if (ok) {
                            const std::string var = m[1].str();
                            bool isConst = false;
                            int idx = -1;
                            if (m[2].matched) {
                                idx = ConstIndex(
                                    m[2].str().substr(1, m[2].str().size() - 2),
                                    isConst);
                            }
                            for (size_t ti : itemsOf(var)) {
                                TextItem& item = model.texts[ti];
                                if (isConst && item.index != idx) {
                                    continue;
                                }
                                for (int k = 0; k < 4; ++k) {
                                    item.color[k] = rgba[k];
                                }
                                item.hasColor = true;
                            }
                        }
                    }
                    adv = closeOff + 1;
                }
            }
            it = sourceText.begin() + (adv <= sourceText.size() ? adv : sourceText.size());
            if (adv >= sourceText.size()) {
                break;
            }
        }
    }
    // 5. renderUI(x, y) sites — last site wins per item; constant numeric
    //    args place the item, anything else marks it dynamic.
    {
        static const std::regex renderCallRe(
            "([A-Za-z_]\\w*)\\s*(\\[[^\\]]*\\])?\\s*\\.\\s*renderUI\\s*\\(");
        std::string::const_iterator it = sourceText.begin();
        std::smatch m;
        while (std::regex_search(it, sourceText.cend(), m, renderCallRe)) {
            const size_t callOff =
                static_cast<size_t>(m.position(0) + (it - sourceText.begin()));
            const size_t openOff = sourceText.find('(', callOff);
            size_t adv = (openOff == std::string::npos)
                             ? sourceText.size()
                             : openOff + 1;
            if (openOff != std::string::npos) {
                const size_t closeOff = MatchBracket(sourceText, openOff, ')');
                if (closeOff != std::string::npos) {
                    const std::string inner =
                        sourceText.substr(openOff + 1, closeOff - openOff - 1);
                    const std::vector<std::string> parts = SplitTopLevel(inner, ',');
                    if (parts.size() == 2) {
                        const std::string var = m[1].str();
                        bool isConst = false;
                        int idx = -1;
                        if (m[2].matched) {
                            idx = ConstIndex(
                                m[2].str().substr(1, m[2].str().size() - 2),
                                isConst);
                        }
                        const std::string xExpr = Trim(parts[0]);
                        const std::string yExpr = Trim(parts[1]);
                        double xv = 0, yv = 0;
                        const bool xNum = ParseDouble(xExpr, xv);
                        const bool yNum = ParseDouble(yExpr, yv);
                        for (size_t ti : itemsOf(var)) {
                            TextItem& item = model.texts[ti];
                            if (isConst && item.index != idx) {
                                continue;
                            }
                            item.xExpr = xExpr;
                            item.yExpr = yExpr;
                            item.xNum = xNum;
                            item.yNum = yNum;
                            if (xNum) {
                                item.xVal = xv;
                            }
                            if (yNum) {
                                item.yVal = yv;
                            }
                            item.dynamicPos = !(xNum && yNum);
                            item.siteOpen = openOff;
                            item.siteClose = closeOff;
                            item.hasSite = true;
                            // Argument spans for the rewrite path (leading
                            // whitespace excluded so splices keep separators).
                            for (int k = 0; k < 2; ++k) {
                                // Re-split: find k-th top-level comma.
                                size_t scan = openOff + 1;
                                int depth = 0, seen = 0;
                                size_t a0 = scan, a1 = closeOff;
                                bool inS = false, inC = false, inL = false,
                                     inB = false;
                                char q = 0;
                                for (; scan < closeOff; ++scan) {
                                    const char c = sourceText[scan];
                                    const char n = scan + 1 < closeOff
                                                       ? sourceText[scan + 1]
                                                       : 0;
                                    if (inL) {
                                        if (c == '\n') {
                                            inL = false;
                                        }
                                        continue;
                                    }
                                    if (inB) {
                                        if (c == '*' && n == '/') {
                                            ++scan;
                                            inB = false;
                                        }
                                        continue;
                                    }
                                    if (inS || inC) {
                                        if (c == '\\' && n) {
                                            ++scan;
                                        } else if (c == q) {
                                            inS = inC = false;
                                        }
                                        continue;
                                    }
                                    if (c == '/' && n == '/') {
                                        inL = true;
                                        ++scan;
                                        continue;
                                    }
                                    if (c == '/' && n == '*') {
                                        inB = true;
                                        ++scan;
                                        continue;
                                    }
                                    if (c == '"' || c == '\'') {
                                        inS = (c == '"');
                                        inC = (c == '\'');
                                        q = c;
                                        continue;
                                    }
                                    if (c == '(' || c == '[' || c == '{') {
                                        ++depth;
                                    } else if (c == ')' || c == ']' || c == '}') {
                                        --depth;
                                    } else if (c == ',' && depth == 0) {
                                        if (seen == k) {
                                            a1 = scan;
                                            break;
                                        }
                                        ++seen;
                                        a0 = scan + 1;
                                    }
                                }
                                while (a0 < a1 &&
                                       std::isspace(static_cast<unsigned char>(
                                           sourceText[a0]))) {
                                    ++a0;
                                }
                                if (k == 0) {
                                    item.arg1Begin = a0;
                                    item.arg1End = a1;
                                } else {
                                    item.arg2Begin = a0;
                                    item.arg2End = a1;
                                }
                            }
                        }
                    }
                    adv = closeOff + 1;
                }
            }
            it = sourceText.begin() + (adv <= sourceText.size() ? adv : sourceText.size());
            if (adv >= sourceText.size()) {
                break;
            }
        }
    }
    // 6. Copy font file/size into items.
    for (const FontDecl& decl : model.fonts) {
        if (!decl.hasFile) {
            continue;
        }
        for (TextItem& item : model.texts) {
            if (item.varName == decl.varName) {
                item.fontFile = decl.file;
                item.pointSize = decl.pointSize;
            }
        }
    }
}

} // namespace detail

bool SetTextPosition(SceneFile& file, const std::string& var, int index,
                     const std::string& x, const std::string& y,
                     std::string& error) {
    TextItem* item = nullptr;
    for (TextItem& cand : file.model.texts) {
        if (cand.varName != var) {
            continue;
        }
        if (index < 0) {
            if (cand.hasSite) {
                item = &cand;
                break;
            }
            if (!item) {
                item = &cand;
            }
        } else if (cand.index == index) {
            item = &cand;
            break;
        }
    }
    if (!item) {
        error = "unknown text item '" + var + "'";
        return false;
    }
    if (item->dynamicPos || !item->hasSite) {
        error = "dynamic layout for '" + var +
                "' — edit the source instead of baking constants";
        return false;
    }
    // Trim trailing whitespace inside each arg span, then splice the later
    // span first so earlier offsets stay valid. Single pass, then reparse.
    size_t a1e = item->arg1End;
    while (a1e > item->arg1Begin &&
           std::isspace(static_cast<unsigned char>(file.sourceText[a1e - 1]))) {
        --a1e;
    }
    size_t a2e = item->arg2End;
    while (a2e > item->arg2Begin &&
           std::isspace(static_cast<unsigned char>(file.sourceText[a2e - 1]))) {
        --a2e;
    }
    // NOTE: arg offsets address file.sourceText, but SpliceRange works on
    // file.sourceLines — both derive from the same text, so convert via a
    // temporary line split of the current text.
    const std::vector<size_t> starts = detail::LineStarts(file.sourceText);
    std::vector<std::string> lines = detail::SplitLines(file.sourceText, file.sourceEol);
    auto toPos = [&](size_t off) {
        const size_t ln = detail::OffsetToLine(starts, off);
        const size_t base = (ln >= 1 && ln - 1 < starts.size()) ? starts[ln - 1] : off;
        return std::make_pair(ln, off >= base ? off - base : size_t(0));
    };
    const auto [l2, c2] = toPos(item->arg2Begin);
    const auto [e2, e2c] = toPos(a2e);
    if (!detail::SpliceRange(lines, l2, c2, e2, e2c, y, error)) {
        return false;
    }
    const auto [l1, c1] = toPos(item->arg1Begin);
    const auto [e1, e1c] = toPos(a1e);
    if (!detail::SpliceRange(lines, l1, c1, e1, e1c, x, error)) {
        return false;
    }
    file.sourceLines = std::move(lines);
    return detail::Reparse(file, error);
}
} // namespace scenecpp
} // namespace studio
} // namespace icg
