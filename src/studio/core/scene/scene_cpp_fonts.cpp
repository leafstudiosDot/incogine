// Incogine Studio — C++ scene font/text-label scan (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Header `Font` declarations plus file-wide setFontFile/setTextContent/
// setColor/renderUI call sites, and the guarded SetTextPosition rewrite.
// Shared helpers: scene_cpp_detail.h. Public API: scene_cpp.h.
#include "scene_cpp.h"
#include "scene_cpp_detail.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

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
        // `Type name[] = { ... };` and bare `name = { ... };` (e.g. static
        // const vectors) — count top-level initializers. Lookups are by
        // exact name, so extra entries are harmless.
        static const std::regex arrayInitRe(
            "([A-Za-z_]\\w*)\\s*(?:\\[\\s*\\])?\\s*=\\s*\\{");
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

// Counted for-loop with a constant-stepped index from a literal start:
// `for (... <var> = <init>; ... <bound-op> <bound>; ...) { body }`.
struct ForLoop {
    std::string var;
    std::string initExpr;
    std::string boundOp; // "<", "<=", "!="
    std::string boundExpr;
    size_t bodyBegin = 0; // absolute offsets of the '{...}' body
    size_t bodyEnd = 0;
};

std::vector<ForLoop> CollectForLoops(const std::string& sourceText) {
    std::vector<ForLoop> loops;
    static const std::regex forRe(
        "for\\s*\\(\\s*(?:int|unsigned|long|std::size_t|size_t|auto)\\s+"
        "([A-Za-z_]\\w*)\\s*=\\s*([^;]+);\\s*\\1\\s*(<|<=|!=)\\s*([^;]+);[^)]*\\)");
    std::string::const_iterator it = sourceText.begin();
    std::smatch m;
    while (std::regex_search(it, sourceText.cend(), m, forRe)) {
        const size_t matchOff =
            static_cast<size_t>(m.position(0) + (it - sourceText.begin()));
        const size_t matchEnd = matchOff + static_cast<size_t>(m.length(0));
        const size_t openOff = sourceText.find('{', matchEnd);
        if (openOff == std::string::npos) {
            it = m.suffix().first;
            if (it == sourceText.cend()) {
                break;
            }
            continue;
        }
        const size_t closeOff = detail::MatchBracket(sourceText, openOff, '}');
        if (closeOff == std::string::npos) {
            it = m.suffix().first;
            if (it == sourceText.cend()) {
                break;
            }
            continue;
        }
        ForLoop loop;
        loop.var = m[1].str();
        loop.initExpr = detail::Trim(m[2].str());
        loop.boundOp = m[3].str();
        loop.boundExpr = detail::Trim(m[4].str());
        loop.bodyBegin = openOff;
        loop.bodyEnd = closeOff + 1;
        loops.push_back(loop);
        it = sourceText.begin() + closeOff + 1;
    }
    return loops;
}

// Raw constexpr values (name -> right-hand side) shared by size resolution
// and the loop evaluator.
std::unordered_map<std::string, std::string> CollectConstexpr(
    const std::string& headerText) {
    std::unordered_map<std::string, std::string> out;
    static const std::regex constexprRe(
        "constexpr\\s+(?:const\\s+)?[A-Za-z_][\\w\\s\\*:]*?\\b([A-Za-z_]\\w*)\\s*="
        "\\s*([^;]+);");
    std::string::const_iterator it = headerText.begin();
    std::smatch m;
    while (std::regex_search(it, headerText.cend(), m, constexprRe)) {
        out[m[1].str()] = detail::Trim(m[2].str());
        it = m.suffix().first;
        if (it == headerText.cend()) {
            break;
        }
    }
    return out;
}

// Top-level element count of a braced initializer (`Name = { ... }`,
// brackets optional so static vectors count too). 0 when absent.
int CountBracedInit(const std::string& headerText, const std::string& arrayName) {
    static const std::regex arrayInitRe(
        "([A-Za-z_]\\w*)\\s*(?:\\[\\s*\\])?\\s*=\\s*\\{");
    std::string::const_iterator it = headerText.begin();
    std::smatch m;
    while (std::regex_search(it, headerText.cend(), m, arrayInitRe)) {
        if (m[1].str() != arrayName) {
            it = m.suffix().first;
            if (it == headerText.cend()) {
                break;
            }
            continue;
        }
        const size_t matchOff =
            static_cast<size_t>(m.position(0) + (it - headerText.begin()));
        const size_t openOff = headerText.find('{', matchOff);
        if (openOff == std::string::npos) {
            return 0;
        }
        const size_t closeOff = detail::MatchBracket(headerText, openOff, '}');
        if (closeOff == std::string::npos || closeOff <= openOff + 1) {
            return 0;
        }
        int count = 0;
        for (const std::string& part : detail::SplitTopLevel(
                 headerText.substr(openOff + 1, closeOff - openOff - 1), ',')) {
            if (!detail::Trim(part).empty()) {
                ++count;
            }
        }
        return (count >= 1 && count <= 4096) ? count : 0;
    }
    return 0;
}

// Top-level string-first elements of a braced initializer: menu item names
// (`SettingsMenu = { { "Video", {...} }, ... }`) or plain string arrays.
std::vector<std::string> ParseMenuNames(const std::string& headerText,
                                        const std::string& arrayName) {
    std::vector<std::string> names;
    static const std::regex arrayInitRe(
        "([A-Za-z_]\\w*)\\s*(?:\\[\\s*\\])?\\s*=\\s*\\{");
    std::string::const_iterator it = headerText.begin();
    std::smatch m;
    while (std::regex_search(it, headerText.cend(), m, arrayInitRe)) {
        if (m[1].str() != arrayName) {
            it = m.suffix().first;
            if (it == headerText.cend()) {
                break;
            }
            continue;
        }
        const size_t matchOff =
            static_cast<size_t>(m.position(0) + (it - headerText.begin()));
        const size_t openOff = headerText.find('{', matchOff);
        if (openOff == std::string::npos) {
            return names;
        }
        const size_t closeOff = detail::MatchBracket(headerText, openOff, '}');
        if (closeOff == std::string::npos) {
            return names;
        }
        static const std::regex firstString("\"((?:[^\"\\\\]|\\\\.)*)\"");
        for (const std::string& part : detail::SplitTopLevel(
                 headerText.substr(openOff + 1, closeOff - openOff - 1), ',')) {
            std::smatch sm;
            const std::string trimmed = detail::Trim(part);
            if (std::regex_search(trimmed, sm, firstString)) {
                names.push_back(sm[1].str());
            }
        }
        return names;
    }
    return names;
}

// Tiny expression evaluator with C++ int/float semantics for layout math:
// literals, identifiers, +-*/%(), unary minus, static_cast<arithmetic>(),
// C-style (arithmetic) casts. Anything else (calls, members) fails.
struct EvalValue {
    bool isFloat = false;
    int64_t i = 0;
    double f = 0.0;
};

struct EvalEnv {
    // Resolved identifiers (loop index, locals, window consts, constexprs).
    std::unordered_map<std::string, EvalValue> vars;
    // Locals whose values derive from GetWindowSize()/getSize() (provenance
    // for constraint detection: `yindex` carries no marker itself).
    std::unordered_map<std::string, bool> windowVars;
    // `<name>.size()` counts for static data arrays.
    std::unordered_map<std::string, int64_t> sizes;
    // Simulated engine window (GetWindowSize reports this).
    int windowWidth = 1280;
    int windowHeight = 720;
    // Glyph measurement + model access for getSize() substitution.
    FontMeasureFn measure = nullptr;
    const SceneModel* model = nullptr;
};

namespace {

struct EvalParser {
    const std::string& text;
    size_t pos = 0;
    const EvalEnv& env;
    bool ok = true;

    explicit EvalParser(const std::string& text, const EvalEnv& env)
        : text(text), env(env) {}

    void skip() {
        while (pos < text.size() &&
               std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
    }

    EvalValue parseExpr() {
        EvalValue left = parseTerm();
        while (ok) {
            skip();
            if (pos >= text.size() || (text[pos] != '+' && text[pos] != '-')) {
                break;
            }
            const char op = text[pos++];
            EvalValue right = parseTerm();
            left = applyBinary(op, left, right);
        }
        return left;
    }

    EvalValue parseTerm() {
        EvalValue left = parseFactor();
        while (ok) {
            skip();
            if (pos >= text.size() ||
                (text[pos] != '*' && text[pos] != '/' && text[pos] != '%')) {
                break;
            }
            const char op = text[pos++];
            EvalValue right = parseFactor();
            if ((op == '%' || op == '/') && !ok) {
                return left;
            }
            if (op == '%' && (left.isFloat || right.isFloat)) {
                ok = false; // C++ rejects % on floats; fail, don't guess
                return left;
            }
            if (op == '/' &&
                ((right.isFloat && right.f == 0.0) ||
                 (!right.isFloat && right.i == 0))) {
                ok = false; // divide by zero: unknown, not zero
                return left;
            }
            left = applyBinary(op, left, right);
        }
        return left;
    }

    EvalValue parseFactor() {
        skip();
        if (pos >= text.size()) {
            ok = false;
            return {};
        }
        if (text[pos] == '(') {
            // C-style cast `(float)x` or parenthesized group.
            size_t save = pos;
            ++pos;
            skip();
            std::string word;
            while (pos < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[pos])) ||
                    text[pos] == '_' || text[pos] == ':')) {
                word += text[pos++];
            }
            skip();
            if ((word == "float" || word == "double" || word == "int" ||
                 word == "long" || word == "unsigned") &&
                pos < text.size() && text[pos] == ')') {
                ++pos;
                EvalValue inner = parseFactor();
                return castTo(inner, word == "float" || word == "double");
            }
            pos = save + 1; // plain group (skip the '(')
            EvalValue inner = parseExpr();
            skip();
            if (pos >= text.size() || text[pos] != ')') {
                ok = false;
                return {};
            }
            ++pos;
            return inner;
        }
        if (text[pos] == '-') {
            ++pos;
            EvalValue inner = parseFactor();
            if (inner.isFloat) {
                inner.f = -inner.f;
            } else {
                inner.i = -inner.i;
            }
            return inner;
        }
        if (text.compare(pos, 11, "static_cast") == 0) {
            pos += 11;
            skip();
            if (pos >= text.size() || text[pos] != '<') {
                ok = false;
                return {};
            }
            ++pos;
            std::string type;
            while (pos < text.size() && text[pos] != '>') {
                type += text[pos++];
            }
            if (pos >= text.size()) {
                ok = false;
                return {};
            }
            ++pos; // '>'
            skip();
            if (pos >= text.size() || text[pos] != '(') {
                ok = false;
                return {};
            }
            ++pos;
            EvalValue inner = parseExpr();
            skip();
            if (pos >= text.size() || text[pos] != ')') {
                ok = false;
                return {};
            }
            ++pos;
            const std::string t = detail::Trim(type);
            return castTo(inner, t == "float" || t == "double");
        }
        if (std::isdigit(static_cast<unsigned char>(text[pos])) ||
            text[pos] == '.') {
            return parseNumber();
        }
        if (std::isalpha(static_cast<unsigned char>(text[pos])) ||
            text[pos] == '_') {
            std::string name;
            while (pos < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[pos])) ||
                    text[pos] == '_')) {
                name += text[pos++];
            }
            skip();
            // `<name>.size()` for counted static arrays.
            if (pos < text.size() && text[pos] == '.') {
                size_t save = pos;
                ++pos;
                std::string member;
                while (pos < text.size() &&
                       (std::isalnum(static_cast<unsigned char>(text[pos])) ||
                        text[pos] == '_')) {
                    member += text[pos++];
                }
                skip();
                if (member == "size" && pos < text.size() && text[pos] == '(') {
                    ++pos;
                    skip();
                    if (pos < text.size() && text[pos] == ')') {
                        ++pos;
                        const auto it = env.sizes.find(name);
                        if (it != env.sizes.end()) {
                            EvalValue v;
                            v.i = it->second;
                            return v;
                        }
                    }
                }
                pos = save;
                ok = false;
                return {};
            }
            const auto it = env.vars.find(name);
            if (it == env.vars.end()) {
                ok = false; // unknown identifier (calls land here too)
                return {};
            }
            return it->second;
        }
        ok = false;
        return {};
    }

    EvalValue parseNumber() {
        size_t start = pos;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        bool isFloat = false;
        if (pos < text.size() && text[pos] == '.') {
            isFloat = true;
            ++pos;
            while (pos < text.size() &&
                   std::isdigit(static_cast<unsigned char>(text[pos]))) {
                ++pos;
            }
        }
        if (pos < text.size() && (text[pos] == 'f' || text[pos] == 'F')) {
            isFloat = true;
            ++pos;
        }
        while (pos < text.size() && (text[pos] == 'u' || text[pos] == 'U' ||
                                     text[pos] == 'l' || text[pos] == 'L')) {
            ++pos; // integer suffixes (100u, 5ull)
        }
        EvalValue v;
        try {
            if (isFloat) {
                v.isFloat = true;
                v.f = std::stod(text.substr(start, pos - start));
            } else {
                v.i = std::stoll(text.substr(start, pos - start));
            }
        } catch (...) {
            ok = false;
        }
        return v;
    }

    static EvalValue castTo(const EvalValue& v, bool toFloat) {
        EvalValue out;
        if (toFloat) {
            out.isFloat = true;
            out.f = v.isFloat ? v.f : static_cast<double>(v.i);
        } else {
            out.i = v.isFloat ? static_cast<int64_t>(v.f) : v.i;
        }
        return out;
    }

    static EvalValue applyBinary(char op, const EvalValue& a, const EvalValue& b) {
        EvalValue out;
        if (a.isFloat || b.isFloat) {
            const double x = a.isFloat ? a.f : static_cast<double>(a.i);
            const double y = b.isFloat ? b.f : static_cast<double>(b.i);
            out.isFloat = true;
            switch (op) {
                case '+': out.f = x + y; break;
                case '-': out.f = x - y; break;
                case '*': out.f = x * y; break;
                case '/': out.f = (y != 0.0) ? x / y : 0.0; break;
                default: out.f = 0.0; break; // '%' on floats fails below
            }
            if (op == '%') {
                out.isFloat = false;
                out.i = 0;
            }
            return out;
        }
        switch (op) {
            case '+': out.i = a.i + b.i; break;
            case '-': out.i = a.i - b.i; break;
            case '*': out.i = a.i * b.i; break;
            case '/': out.i = (b.i != 0) ? a.i / b.i : 0; break; // truncates
            case '%': out.i = (b.i != 0) ? a.i % b.i : 0; break;
            default: break;
        }
        return out;
    }
};

} // namespace

// Replaces Engine::...->GetWindowSize().width|height with the simulated
// window dimensions so layout math evaluates as the running game computes.
std::string NormalizeWindowAccess(std::string expr, int windowWidth,
                                  int windowHeight) {
    static const std::regex getSizeRe(
        "Engine\\s*::\\s*Instance\\s*\\([^)]*\\)\\s*->\\s*GetWindowSize\\s*\\("
        "\\s*\\)\\s*\\.\\s*(width|height)");
    std::smatch m;
    std::string out;
    std::string::const_iterator it = expr.begin();
    while (std::regex_search(it, expr.cend(), m, getSizeRe)) {
        out.append(it, m[0].first);
        out += (m[1].str() == "width" ? std::to_string(windowWidth)
                                      : std::to_string(windowHeight));
        it = m.suffix().first;
    }
    out.append(it, expr.cend());
    return out;
}

bool EvalExpr(const std::string& expr, const EvalEnv& env, EvalValue& out) {
    const std::string normalized =
        NormalizeWindowAccess(expr, env.windowWidth, env.windowHeight);
    EvalParser parser(normalized, env);
    out = parser.parseExpr();
    parser.skip();
    return parser.ok && parser.pos == normalized.size();
}

std::string NumStr(double v) {
    std::ostringstream oss;
    oss << std::setprecision(6) << std::noshowpoint << v;
    return oss.str();
}

// True when an expression is window-relative: direct GetWindowSize() /
// getSize() markers, or identifiers bound from window-relative locals
// (provenance tracked in env.windowVars, e.g. `yindex`).
bool IsWindowRelative(const std::string& expr, const EvalEnv& env) {
    if (expr.find("GetWindowSize") != std::string::npos ||
        expr.find("getSize") != std::string::npos) {
        return true;
    }
    static const std::regex identRe("[A-Za-z_]\\w*");
    std::string::const_iterator it = expr.begin();
    std::smatch m;
    while (std::regex_search(it, expr.cend(), m, identRe)) {
        if (env.windowVars.find(m[0].str()) != env.windowVars.end()) {
            return true;
        }
        it = m.suffix().first;
        if (it == expr.cend()) {
            break;
        }
    }
    return false;
}

// Replaces `VAR.getSize().width|height` (bare Font or `VAR[const index]`)
// with measured engine px. Works on a copy: false leaves expr untouched.
bool SubstituteFontSizes(std::string& expr, const EvalEnv& env) {
    if (expr.find("getSize") == std::string::npos || !env.measure ||
        !env.model) {
        return expr.find("getSize") == std::string::npos;
    }
    static const std::regex getSizeRe(
        "([A-Za-z_]\\w*)\\s*(\\[[^\\]]+\\])?\\s*\\.\\s*getSize\\s*\\(\\s*\\)"
        "\\s*\\.\\s*(width|height)");
    std::string work = expr;
    for (int pass = 0; pass < 8; ++pass) {
        std::smatch m;
        if (!std::regex_search(work, m, getSizeRe)) {
            expr = work;
            return true;
        }
        const std::string var = m[1].str();
        const bool wantW = (m[3].str() == "width");
        // Owning declaration for the raster size.
        const FontDecl* decl = nullptr;
        for (const FontDecl& d : env.model->fonts) {
            if (d.varName == var) {
                decl = &d;
                break;
            }
        }
        if (!decl || !decl->hasFile || decl->pointSize <= 0) {
            return false;
        }
        // Content of the measured item (index-resolved for arrays).
        int itemIdx = -1;
        if (m[2].matched) {
            EvalValue iv;
            if (!EvalExpr(detail::Trim(m[2].str().substr(
                                          1, m[2].str().size() - 2)),
                          env, iv) ||
                iv.isFloat) {
                return false;
            }
            itemIdx = static_cast<int>(iv.i);
        }
        std::string content;
        bool found = false;
        for (const TextItem& t : env.model->texts) {
            if (t.varName == var && t.index == itemIdx) {
                content = t.content;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
        double w = 0, h = 0;
        if (!env.measure(decl->file, decl->pointSize, env.windowHeight,
                         content, w, h)) {
            return false;
        }
        work.replace(static_cast<size_t>(m.position(0)),
                     static_cast<size_t>(m.length(0)),
                     NumStr(wantW ? w : h));
    }
    return false; // still nested after 8 passes: give up
}

// Layout evaluation: getSize() substitution first, then the evaluator.
// Window-relative expressions evaluate to numbers here; constraint-ness
// is tracked separately (pins/provenance), not by failure.
bool EvalLayoutExpr(const std::string& expr, const EvalEnv& env,
                    EvalValue& out) {
    std::string substituted = expr;
    if (!SubstituteFontSizes(substituted, env)) {
        return false;
    }
    return EvalExpr(substituted, env, out);
}

// Edge pins for the constraint toggles, from the raw winning arg text
// (whitespace-insensitive). Canonical Studio forms pin exactly; other
// window-relative code leaves pins empty (custom constraint: toggles
// enabled, all off). Plain numeric constants pin left/top (absolute
// coords are origin-relative by definition).
void DetectPins(const std::string& rawArg, const std::string& var, int index,
                bool isX, std::string& pin) {
    std::string flat;
    flat.reserve(rawArg.size());
    for (char c : rawArg) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            flat += c;
        }
    }
    const std::string dim = isX ? "width" : "height";
    const std::string edge = isX ? "right" : "bottom";
    const std::string mid = "center";
    const std::string home = isX ? "left" : "top";
    // Canonical: Engine::Instance(0, nullptr)->GetWindowSize().{dim} (-
    // VAR[idx].getSize().{dim} (2 - ...)?)? with optional numeric margin.
    const std::string getSize =
        std::string("Engine::Instance\\(0,nullptr\\)->GetWindowSize\\(\\)\\.") +
        dim;
    // Build with the concrete variable (identifiers are regex-safe).
    const std::string varPart = var + "(\\[[^\\]]+\\])?";
    std::smatch m;
    const std::regex centerRe("^" + getSize + "/2-" + varPart +
                              "\\.getSize\\(\\)\\." + dim + "/2([+-][0-9.]+)?$");
    if (std::regex_match(flat, m, centerRe)) {
        if ((!m[1].matched && index < 0) || (m[1].matched && [&] {
                bool isConst = false;
                return ConstIndex(
                           m[1].str().substr(1, m[1].str().size() - 2),
                           isConst) == index &&
                       isConst;
            }())) {
            pin = mid;
            return;
        }
    }
    const std::regex edgeRe("^" + getSize + "-" + varPart +
                            "\\.getSize\\(\\)\\." + dim + "([+-][0-9.]+)?$");
    if (std::regex_match(flat, m, edgeRe)) {
        if ((!m[1].matched && index < 0) || (m[1].matched && [&] {
                bool isConst = false;
                return ConstIndex(
                           m[1].str().substr(1, m[1].str().size() - 2),
                           isConst) == index &&
                       isConst;
            }())) {
            pin = edge;
            return;
        }
    }
    if (flat.find("GetWindowSize()." + dim) != std::string::npos) {
        // Custom window-relative code (e.g. via locals): constrained, but
        // the pin matcher doesn't name it — toggles stay convertible.
        pin.clear();
        return;
    }
    double v = 0;
    if (detail::ParseDouble(detail::Trim(rawArg), v)) {
        pin = home;
        return;
    }
    pin.clear();
}

// `<name>.size()` counts for static data arrays in the header (e.g.
// SettingsMenu -> 5 via its braced initializer). Used both for
// `v.resize(Data.size())` sizing and loop-bound evaluation.
std::unordered_map<std::string, int64_t> BuildSizes(const std::string& headerText) {
    std::unordered_map<std::string, int64_t> sizes;
    static const std::regex arrayInitRe(
        "([A-Za-z_]\\w*)\\s*(?:\\[\\s*\\])?\\s*=\\s*\\{");
    std::string::const_iterator it = headerText.begin();
    std::smatch m;
    while (std::regex_search(it, headerText.cend(), m, arrayInitRe)) {
        const std::string name = m[1].str();
        if (sizes.find(name) == sizes.end()) {
            const int n = CountBracedInit(headerText, name);
            if (n >= 1 && n <= 4096) {
                sizes[name] = n;
            }
        }
        it = m.suffix().first;
        if (it == headerText.cend()) {
            break;
        }
    }
    return sizes;
}

// Base environment: static `.size()` counts plus plain-numeric constexprs
// (including the `sizeof(Arr)/sizeof(Arr[0])` idiom via the counts above).
// GetWindowSize() evaluates to the simulated window.
EvalEnv BuildBaseEnv(const std::string& headerText, WindowSize window,
                     const SceneModel& model, FontMeasureFn measure) {
    EvalEnv env;
    env.windowWidth = window.width;
    env.windowHeight = window.height;
    env.model = &model;
    env.measure = measure;
    env.sizes = BuildSizes(headerText);
    static const std::regex sizeofRe(
        "sizeof\\s*\\(\\s*([A-Za-z_]\\w*)\\s*\\)\\s*/\\s*sizeof\\s*\\(\\s*\\1\\s*\\[0\\]\\s*\\)");
    for (const auto& kv : CollectConstexpr(headerText)) {
        EvalValue v;
        EvalEnv empty;
        empty.sizes = env.sizes;
        if (EvalExpr(kv.second, empty, v)) {
            env.vars[kv.first] = v;
            continue;
        }
        std::smatch m;
        const std::string rhs = detail::Trim(kv.second);
        if (std::regex_match(rhs, m, sizeofRe)) {
            const auto ai = env.sizes.find(m[1].str());
            if (ai != env.sizes.end()) {
                EvalValue sv;
                sv.i = ai->second;
                env.vars[kv.first] = sv;
            }
        }
    }
    return env;
}

// Iteration count for a counted loop, or -1 when the bound is dynamic.
// Caps at 256 to bound pathological expansion.
int LoopRange(const ForLoop& loop, const EvalEnv& base) {
    EvalValue initV, boundV;
    if (!EvalExpr(loop.initExpr, base, initV) || initV.isFloat) {
        return -1;
    }
    if (!EvalExpr(loop.boundExpr, base, boundV) || boundV.isFloat) {
        return -1;
    }
    const int64_t init = initV.i;
    const int64_t bound = boundV.i;
    int64_t count = -1;
    if (loop.boundOp == "<") {
        count = bound - init;
    } else if (loop.boundOp == "<=") {
        count = bound - init + 1;
    } else if (loop.boundOp == "!=") {
        count = bound - init; // constant-stepped forward loops only
    }
    if (count < 0 || count > 256) {
        return -1;
    }
    return static_cast<int>(count);
}

// Innermost loop containing absolute offset off, or nullptr.
const ForLoop* EnclosingLoop(const std::vector<ForLoop>& loops, size_t off) {
    const ForLoop* best = nullptr;
    for (const ForLoop& loop : loops) {
        if (off >= loop.bodyBegin && off < loop.bodyEnd) {
            if (!best || (loop.bodyEnd - loop.bodyBegin) <
                             (best->bodyEnd - best->bodyBegin)) {
                best = &loop;
            }
        }
    }
    return best;
}

// Evaluates `int|float|double|auto name = <expr>;` declarations in
// [scopeBegin, callOff) in source order, threading the environment so
// later locals see earlier ones (and the loop index when set). `for`
// headers are skipped. int-ish types truncate floats (C++ semantics).
void ApplyVisibleLocals(const std::string& sourceText, size_t scopeBegin,
                        size_t callOff, EvalEnv& env) {
    if (scopeBegin >= callOff || callOff > sourceText.size()) {
        return;
    }
    static const std::regex localRe(
        "(?:^|;|\\{|\\})\\s*(?:const\\s+)?(int|float|double|auto|unsigned|long|"
        "std::size_t|size_t)\\s+([A-Za-z_]\\w*)\\s*=\\s*([^;]+);");
    // Strip comments so trailing `// ...` notes don't break the `;`-to-
    // next-decl anchor (offsets unused here, only declaration order).
    const std::string window = detail::StripComments(
        sourceText.substr(scopeBegin, callOff - scopeBegin));
    std::string::const_iterator it = window.begin();
    std::smatch m;
    while (std::regex_search(it, window.cend(), m, localRe)) {
        EvalValue v;
        const std::string rhs = detail::Trim(m[3].str());
        // Provenance independent of evaluation success: `yindex` stays
        // window-relative even when the loop index is unbound (fallback).
        if (IsWindowRelative(rhs, env)) {
            env.windowVars[m[2].str()] = true;
        }
        // Layout evaluation (getSize() resolves via env.measure/model).
        if (EvalLayoutExpr(rhs, env, v)) {
            const std::string type = m[1].str();
            const bool intType =
                (type == "int" || type == "long" || type == "unsigned" ||
                 type == "size_t" || type == "std::size_t");
            const bool floatType = (type == "float" || type == "double");
            if (intType && v.isFloat) {
                EvalValue t;
                t.i = static_cast<int64_t>(v.f); // C++ truncates toward zero
                v = t;
            } else if (floatType && !v.isFloat) {
                EvalValue t;
                t.isFloat = true;
                t.f = static_cast<double>(v.i);
                v = t;
            }
            env.vars[m[2].str()] = v;
        }
        it = m.suffix().first;
        if (it == window.cend()) {
            break;
        }
    }
}

// Records renderUI argument spans (leading whitespace excluded) for the
// rewrite path. Shared with both the constant and loop-evaluated paths.
void RecordSiteSpans(const std::string& sourceText, size_t openOff,
                     size_t closeOff, TextItem& item) {
    for (int k = 0; k < 2; ++k) {
        size_t scan = openOff + 1;
        int depth = 0, seen = 0;
        size_t a0 = scan, a1 = closeOff;
        bool inS = false, inC = false, inL = false, inB = false;
        char q = 0;
        for (; scan < closeOff; ++scan) {
            const char c = sourceText[scan];
            const char n =
                scan + 1 < closeOff ? sourceText[scan + 1] : 0;
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
               std::isspace(static_cast<unsigned char>(sourceText[a0]))) {
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

// Fills a winning renderUI site: evaluated values, arg spans, rewrite
// guards, and constraint/pin metadata from the raw args (env carries the
// locals provenance for transitively window-relative expressions).
void PlaceItemAt(const std::string& sourceText, size_t openOff, size_t closeOff,
                 const std::string& var, const std::string& xExpr,
                 const std::string& yExpr, double xVal, double yVal,
                 bool shared, const EvalEnv& env, TextItem& item) {
    item.xExpr = xExpr;
    item.yExpr = yExpr;
    item.xNum = true;
    item.yNum = true;
    item.xVal = xVal;
    item.yVal = yVal;
    item.dynamicPos = false;
    item.siteOpen = openOff;
    item.siteClose = closeOff;
    item.hasSite = true;
    item.sharedSite = shared;
    item.hasConstraint =
        IsWindowRelative(xExpr, env) || IsWindowRelative(yExpr, env);
    DetectPins(xExpr, var, item.index, true, item.xPin);
    DetectPins(yExpr, var, item.index, false, item.yPin);
    RecordSiteSpans(sourceText, openOff, closeOff, item);
}

} // namespace

namespace detail {

void ParseFonts(const std::string& headerText, const std::string& sourceText,
                SceneModel& model, WindowSize window, FontMeasureFn measure) {
    const std::vector<size_t> srcStarts = LineStarts(sourceText);
    std::unordered_map<std::string, size_t> fontIndex;

    // 1. Member declarations from the header: `Font f;`, `Font f[N];`
    //    (literal/constexpr/sizeof sizes), and `std::vector<Font> v;`
    //    (sized later by a `v.resize(...)` scan). Anything else unknown.
    {
        static const std::regex declRe(
            "^\\s*Font\\s+([A-Za-z_]\\w*)\\s*(?:\\[\\s*([^\\]]+)\\s*\\])?\\s*;");
        static const std::regex vectorDeclRe(
            "^\\s*(?:std::)?vector\\s*<\\s*Font\\s*>\\s+([A-Za-z_]\\w*)\\s*;");
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
            } else if (std::regex_match(line, m, vectorDeclRe)) {
                FontDecl decl;
                decl.varName = m[1].str();
                decl.isVector = true;
                // Sized below by `v.resize(...)`; items added then.
                fontIndex[decl.varName] = model.fonts.size();
                model.fonts.push_back(decl);
            }
            if (eol == headerText.size()) {
                break;
            }
            start = eol + 1;
        }
    }
    // 1b. `std::vector<Font>` sizing via `v.resize(<count>)` with the same
    //     evaluator used for loop bounds (literals, constexprs, sizeof
    //     idiom counts, `Data.size()`). Unresolvable vectors keep zero
    //     items (e.g. submenu sized from a runtime selection).
    {
        const EvalEnv base = BuildBaseEnv(headerText, window, model, measure);
        static const std::regex resizeCallRe(
            "([A-Za-z_]\\w*)\\s*\\.\\s*resize\\s*\\(");
        std::string::const_iterator it = sourceText.begin();
        std::smatch m;
        while (std::regex_search(it, sourceText.cend(), m, resizeCallRe)) {
            const size_t callOff =
                static_cast<size_t>(m.position(0) + (it - sourceText.begin()));
            const size_t openOff = sourceText.find('(', callOff);
            size_t adv = (openOff == std::string::npos)
                             ? sourceText.size()
                             : openOff + 1;
            if (openOff != std::string::npos) {
                const size_t closeOff =
                    detail::MatchBracket(sourceText, openOff, ')');
                if (closeOff != std::string::npos) {
                    const auto fit = fontIndex.find(m[1].str());
                    if (fit != fontIndex.end()) {
                        FontDecl& decl = model.fonts[fit->second];
                        if (decl.isVector && !decl.isArray) {
                            EvalValue v;
                            if (EvalExpr(
                                    detail::Trim(sourceText.substr(
                                        openOff + 1,
                                        closeOff - openOff - 1)),
                                    base, v) &&
                                !v.isFloat && v.i >= 1 && v.i <= 4096) {
                                decl.isArray = true;
                                decl.arraySize = static_cast<int>(v.i);
                                for (int i = 0; i < decl.arraySize; ++i) {
                                    TextItem item;
                                    item.varName = decl.varName;
                                    item.index = i;
                                    model.texts.push_back(item);
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
    // 4. Numeric setColor(r, g, b, a) — last wins per item. Per-iteration
    //    colors inside loops depend on runtime selection state, so
    //    variable-index calls enclosed in a loop are skipped (items keep
    //    their default rather than baking one branch for all).
    {
        const std::vector<ForLoop> colorLoops = CollectForLoops(sourceText);
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
                            const bool inLoop =
                                EnclosingLoop(colorLoops, callOff) != nullptr;
                            const bool skipLoopColor =
                                inLoop && m[2].matched && !isConst;
                            if (!skipLoopColor) {
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
    // 4b. Data-driven setTextContent(Data[i].name) inside counted loops:
    //     resolves per-iteration content from header initializers
    //     (e.g. `menuFonts[i].setTextContent(SettingsMenu[i].name)`).
    //     Dynamic aliases (submenu selections) stay unknown.
    {
        const EvalEnv base = BuildBaseEnv(headerText, window, model, measure);
        const std::vector<ForLoop> textLoops = CollectForLoops(sourceText);
        std::unordered_map<std::string, std::vector<std::string>> menuCache;
        static const std::regex textCallRe(
            "([A-Za-z_]\\w*)\\s*(\\[[^\\]]*\\])?\\s*\\.\\s*setTextContent\\s*\\(");
        std::string::const_iterator it = sourceText.begin();
        std::smatch m;
        while (std::regex_search(it, sourceText.cend(), m, textCallRe)) {
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
                        Trim(sourceText.substr(openOff + 1,
                                               closeOff - openOff - 1));
                    // Skip literals (handled above); look for Data[idx].name
                    // (struct arrays) or Data[idx] (plain string arrays).
                    static const std::regex dataRe(
                        "^([A-Za-z_]\\w*)\\s*\\[([^\\]]+)\\]\\s*(?:\\.\\s*name"
                        "\\s*)?$");
                    std::smatch dm;
                    if (std::regex_match(inner, dm, dataRe)) {
                        const std::string dataName = dm[1].str();
                        const std::string idxExpr = Trim(dm[2].str());
                        const ForLoop* loop =
                            EnclosingLoop(textLoops, callOff);
                        if (loop != nullptr) {
                            const int count = LoopRange(*loop, base);
                            if (count > 0) {
                                auto mc = menuCache.find(dataName);
                                if (mc == menuCache.end()) {
                                    menuCache[dataName] =
                                        ParseMenuNames(headerText, dataName);
                                    mc = menuCache.find(dataName);
                                }
                                if (static_cast<int>(mc->second.size()) >=
                                    count) {
                                    EvalValue initV;
                                    if (EvalExpr(loop->initExpr, base, initV) &&
                                        !initV.isFloat) {
                                        const std::string targetVar =
                                            m[1].str();
                                        std::string targetIdxExpr;
                                        if (m[2].matched) {
                                            const std::string b =
                                                m[2].str();
                                            targetIdxExpr = Trim(
                                                b.substr(1, b.size() - 2));
                                        }
                                        for (int k = 0; k < count; ++k) {
                                            EvalEnv env = base;
                                            env.vars[loop->var].i =
                                                initV.i + k;
                                            EvalValue dataIdx, targetIdx;
                                            if (!EvalExpr(idxExpr, env,
                                                          dataIdx) ||
                                                dataIdx.isFloat) {
                                                continue;
                                            }
                                            int itemIdx = -1;
                                            if (targetIdxExpr.empty()) {
                                                // Scalar target reused across
                                                // iterations: last wins.
                                                for (size_t ti :
                                                     itemsOf(targetVar)) {
                                                    TextItem& item =
                                                        model.texts[ti];
                                                    const int64_t di =
                                                        dataIdx.i - initV.i;
                                                    if (di >= 0 &&
                                                        di < static_cast<
                                                            int64_t>(
                                                            mc->second
                                                                .size())) {
                                                        item.content =
                                                            mc->second[
                                                                static_cast<
                                                                    size_t>(
                                                                    di)];
                                                        item.hasContent = true;
                                                    }
                                                }
                                                continue;
                                            }
                                            if (!EvalExpr(targetIdxExpr, env,
                                                          targetIdx) ||
                                                targetIdx.isFloat) {
                                                continue;
                                            }
                                            itemIdx = static_cast<int>(
                                                targetIdx.i);
                                            for (size_t ti :
                                                 itemsOf(targetVar)) {
                                                TextItem& item =
                                                    model.texts[ti];
                                                if (item.index != itemIdx) {
                                                    continue;
                                                }
                                                const int64_t di =
                                                    dataIdx.i - initV.i;
                                                // Common case: same index
                                                // into data and fonts.
                                                if (di >= 0 &&
                                                    di < static_cast<int64_t>(
                                                        mc->second.size())) {
                                                    item.content =
                                                        mc->second[static_cast<
                                                            size_t>(di)];
                                                    item.hasContent = true;
                                                }
                                            }
                                        }
                                    }
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
    // 5. renderUI(x, y) sites — last site wins per item; constant numeric
    //    args place the item directly, counted-loop bodies evaluate
    //    per-iteration (locals + loop index, 1280x720 design constants),
    //    anything else marks the item dynamic. Loop-placed items share
    //    one site: drawn, but SetTextPosition refuses (sharedSite).
    {
        const EvalEnv base = BuildBaseEnv(headerText, window, model, measure);
        const std::vector<ForLoop> renderLoops = CollectForLoops(sourceText);
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
                        std::string bracketExpr;
                        if (m[2].matched) {
                            const std::string b = m[2].str();
                            bracketExpr =
                                Trim(b.substr(1, b.size() - 2));
                            idx = ConstIndex(bracketExpr, isConst);
                        }
                        const std::string xExpr = Trim(parts[0]);
                        const std::string yExpr = Trim(parts[1]);
                        const ForLoop* loop =
                            EnclosingLoop(renderLoops, callOff);
                        bool handledLoop = false;
                        if (loop != nullptr) {
                            const int count = LoopRange(*loop, base);
                            EvalValue initV;
                            if (count > 0 &&
                                EvalExpr(loop->initExpr, base, initV) &&
                                !initV.isFloat) {
                                // Scope start: enclosing function body when
                                // findable, else a bounded window before the
                                // loop (avoids leaking prior functions'
                                // locals while keeping preamble locals).
                                size_t scopeBegin = 0;
                                if (loop->bodyBegin > 4000) {
                                    scopeBegin = loop->bodyBegin - 4000;
                                }
                                // Prefer the enclosing '{' of the function:
                                // last line-start '{' before the loop that
                                // closes after the call is complex, so the
                                // bounded window above suffices for v1.
                                int placed = 0;
                                for (int k = 0; k < count; ++k) {
                                    EvalEnv env = base;
                                    env.vars[loop->var].i = initV.i + k;
                                    ApplyVisibleLocals(sourceText, scopeBegin,
                                                       callOff, env);
                                    EvalValue xv, yv;
                                    if (!EvalLayoutExpr(xExpr, env, xv) ||
                                        !EvalLayoutExpr(yExpr, env, yv)) {
                                        continue;
                                    }
                                    const double xNum =
                                        xv.isFloat ? xv.f
                                                   : static_cast<double>(xv.i);
                                    const double yNum =
                                        yv.isFloat ? yv.f
                                                   : static_cast<double>(yv.i);
                                    int itemIdx = -1;
                                    if (bracketExpr.empty()) {
                                        // Scalar reused in a loop: place the
                                        // single item on last iteration.
                                        for (size_t ti : itemsOf(var)) {
                                            TextItem& item = model.texts[ti];
                                            PlaceItemAt(sourceText, openOff,
                                                        closeOff, var, xExpr,
                                                        yExpr, xNum, yNum,
                                                        (count > 1), env, item);
                                        }
                                        ++placed;
                                        continue;
                                    }
                                    EvalValue bv;
                                    if (!EvalExpr(bracketExpr, env, bv) ||
                                        bv.isFloat) {
                                        continue;
                                    }
                                    itemIdx = static_cast<int>(bv.i);
                                    for (size_t ti : itemsOf(var)) {
                                        TextItem& item = model.texts[ti];
                                        if (item.index != itemIdx) {
                                            continue;
                                        }
                                        PlaceItemAt(sourceText, openOff,
                                                    closeOff, var, xExpr, yExpr,
                                                    xNum, yNum, true, env, item);
                                        ++placed;
                                    }
                                }
                                handledLoop = (placed > 0);
                            }
                        }
                        if (!handledLoop) {
                        // Non-loop sites: evaluate against locals + the
                        // simulated window (getSize() via measurement).
                        // Anything unresolvable stays dynamic.
                        EvalEnv callEnv = base;
                        // Bounded window (same as the loop path):
                        // preamble locals without leaking distant
                        // functions' same-named values.
                        const size_t scopeBegin =
                            (callOff > 4000) ? callOff - 4000 : 0;
                        ApplyVisibleLocals(sourceText, scopeBegin, callOff,
                                           callEnv);
                        EvalValue xev, yev;
                        const bool xOk =
                            EvalLayoutExpr(xExpr, callEnv, xev);
                        const bool yOk =
                            EvalLayoutExpr(yExpr, callEnv, yev);
                        for (size_t ti : itemsOf(var)) {
                            TextItem& item = model.texts[ti];
                            if (isConst && item.index != idx) {
                                continue;
                            }
                            // Variable-index non-loop sites stay last-wins
                            // (existing behavior for scalar reuse).
                            if (xOk && yOk) {
                                PlaceItemAt(sourceText, openOff, closeOff, var,
                                            xExpr, yExpr,
                                            xev.isFloat
                                                ? xev.f
                                                : static_cast<double>(xev.i),
                                            yev.isFloat
                                                ? yev.f
                                                : static_cast<double>(yev.i),
                                            false, callEnv, item);
                                continue;
                            }
                            item.xExpr = xExpr;
                            item.yExpr = yExpr;
                            item.xNum = false;
                            item.yNum = false;
                            item.dynamicPos = true;
                            item.siteOpen = openOff;
                            item.siteClose = closeOff;
                            item.hasSite = true;
                            item.hasConstraint =
                                IsWindowRelative(xExpr, callEnv) ||
                                IsWindowRelative(yExpr, callEnv);
                            DetectPins(xExpr, var, item.index, true, item.xPin);
                            DetectPins(yExpr, var, item.index, false, item.yPin);
                            RecordSiteSpans(sourceText, openOff, closeOff,
                                            item);
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
    if (item->hasConstraint) {
        error = "constrained layout for '" + var +
                "' — unconstrain with the edge toggles or edit the source";
        return false;
    }
    if (item->sharedSite) {
        error = "shared loop layout for '" + var +
                "' — one edit would move siblings; edit the source instead";
        return false;
    }
    return SetTextExpression(file, var, item->index, x, y, error);
}

bool SetTextExpression(SceneFile& file, const std::string& var, int index,
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
    if (!item->hasSite) {
        error = "no renderUI site for '" + var +
                "' — edit the source instead of baking constants";
        return false;
    }
    if (item->sharedSite) {
        error = "shared loop layout for '" + var +
                "' — one edit would move siblings; edit the source instead";
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
