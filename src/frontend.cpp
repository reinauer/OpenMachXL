#include "frontend.hpp"
#include "logic.hpp"
#include "device_family.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iomanip>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace machxl {
namespace {
std::string trim(std::string s)
{
    auto first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
std::string upper(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
[[noreturn]] void fail(const std::string& text)
{
    throw LogicError(text);
}

// Patterns used once per source line or per expression. A std::regex is
// costly to construct, so each is built on first use and kept.
const std::regex& index_suffix_pattern()
{
    static const std::regex pattern(R"(\[\d+\])");
    return pattern;
}
const std::regex& bare_number_pattern()
{
    static const std::regex pattern(R"(\b[0-9]+\b)");
    return pattern;
}
const std::regex& inline_declaration_pattern()
{
    static const std::regex pattern(R"([ \t]+(PIN|NODE|GROUP)(?=[ \t?]))", std::regex::icase);
    return pattern;
}
const std::regex& glued_location_pattern()
{
    static const std::regex pattern(R"(^(PIN|NODE)\?)", std::regex::icase);
    return pattern;
}
const std::regex& space_after_comma_pattern()
{
    static const std::regex pattern(R"(,[ \t]+)");
    return pattern;
}
const std::regex& space_after_bracket_pattern()
{
    static const std::regex pattern(R"(\[[ \t]+)");
    return pattern;
}
const std::regex& space_before_bracket_pattern()
{
    static const std::regex pattern(R"([ \t]+\])");
    return pattern;
}

const std::map<std::string, std::pair<unsigned, std::string>>& frontend_devices()
{
    static const std::map<std::string, std::pair<unsigned, std::string>> devices = {
#include "frontend_devices.inc"
    };
    return devices;
}

void validate_declared_name(const std::string& name, const std::string& context)
{
    static const std::set<std::string> reserved = {
#include "frontend_keywords.inc"
    };
    auto base = name;
    if (!base.empty() && base.front() == '/')
        base.erase(0, 1);
    base = base.substr(0, base.find('['));
    if (!reserved.count(base))
        return;
    // Identifier rules depend on the declaration context.
    // Power pins and unconnected locations are legal declarations;
    // string metadata also accepts these two otherwise reserved tokens.
    if (context == "PIN" &&
        (name == "GND" || name == "NC" || name == "VCC" || name == "VCC1" || name == "VCC2" || name == "VCC3"))
        return;
    if (context == "NODE" && name == "NC")
        return;
    if (context == "STRING" && (name == "COLLECTOR" || name == "SIGNATURE"))
        return;
    fail("Reserved keyword cannot be used as " + context + " name: " + name);
}

std::vector<std::string> expand_signal_names(const std::string& text)
{
    static const std::regex pattern(R"(([A-Z_][A-Z0-9_]*)(?:\[([0-9.,]+)\])?)");
    std::smatch m;
    if (!std::regex_match(text, m, pattern))
        fail("Unsupported signal: " + text);
    if (!m[2].matched)
        return {text};
    std::vector<std::string> result;
    std::stringstream parts(m[2]);
    std::string part;
    while (std::getline(parts, part, ',')) {
        static const std::regex range(R"((\d+)(?:\.\.(\d+))?)");
        std::smatch r;
        if (!std::regex_match(part, r, range))
            fail("Invalid vector range: " + text);
        // Longer numerals cannot be in range and would overflow the conversion.
        if (r[1].length() > 5 || r[2].length() > 5)
            fail("Vector range exceeds frontend limit: " + text);
        int first = std::stoi(r[1]), last = r[2].matched ? std::stoi(r[2]) : first;
        if (first > 65535 || last > 65535 || std::abs(first - last) > 255)
            fail("Vector range exceeds frontend limit: " + text);
        int step = first <= last ? 1 : -1;
        for (int n = first;; n += step) {
            if (result.size() >= 256)
                fail("Vector exceeds 256 elements");
            result.push_back(m[1].str() + "[" + std::to_string(n) + "]");
            if (n == last)
                break;
        }
    }
    return result;
}

std::string expand_aliases(const std::string& text, const std::map<std::string, std::string>& aliases,
                           std::set<std::string> active = {})
{
    static const std::regex word(R"([A-Z0-9_]+)");
    if (active.size() > 128)
        fail("STRING expansion nesting exceeds 128");
    std::string out;
    std::size_t previous = 0;
    for (std::sregex_iterator i(text.begin(), text.end(), word), end; i != end; ++i) {
        auto match = *i;
        out += text.substr(previous, match.position() - previous);
        auto found = aliases.find(match.str());
        if (found == aliases.end())
            out += match.str();
        else {
            if (active.count(found->first))
                fail("Recursive STRING: " + found->first);
            auto next = active;
            next.insert(found->first);
            // STRING is literal substitution: adding parentheses changes semantics.
            out += expand_aliases(found->second, aliases, next);
        }
        if (out.size() > 1024 * 1024)
            fail("STRING expansion exceeds 1 MiB");
        previous = match.position() + match.length();
    }
    return out + text.substr(previous);
}

std::string select_vector_element(const std::string& text, std::size_t index, std::size_t width)
{
    static const std::regex vector(R"([A-Z_][A-Z0-9_]*\[[0-9.,]+\])");
    std::string out;
    std::size_t previous = 0;
    for (std::sregex_iterator i(text.begin(), text.end(), vector), end; i != end; ++i) {
        auto m        = *i;
        auto expanded = expand_signal_names(m.str());
        if (expanded.size() != 1 && expanded.size() != width)
            fail("Vector width mismatch in " + m.str());
        out += text.substr(previous, m.position() - previous);
        out += expanded[expanded.size() == 1 ? 0 : index];
        previous = m.position() + m.length();
    }
    return out + text.substr(previous);
}
struct Token
{
    std::string text;
    std::size_t offset;
};
std::vector<Token> tokenize(const std::string& s)
{
    std::vector<Token> result;
    for (std::size_t i = 0; i < s.size();) {
        if (std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
            continue;
        }
        auto start = i;
        if (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '#') {
            while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '#'))
                ++i;
            if (i < s.size() && s[i] == '[') {
                auto close = s.find(']', i);
                if (close == std::string::npos)
                    fail("Unclosed vector");
                i = close + 1;
            }
        } else {
            bool found = false;
            for (auto op : {":+:", ":*:", "+->", ":=", "*=", "..", "->"}) {
                if (s.compare(i, std::char_traits<char>::length(op), op) == 0) {
                    i += std::char_traits<char>::length(op);
                    found = true;
                    break;
                }
            }
            if (!found)
                ++i;
        }
        auto token = s.substr(start, i - start);
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char c) { return std::isspace(c); }),
                    token.end());
        result.push_back({token, start});
    }
    result.push_back({"", s.size()});
    return result;
}
ExprPtr negate(ExprPtr e)
{
    if (e->kind == ExprKind::Constant)
        return make_constant(!e->value);
    if (e->kind == ExprKind::Not)
        return e->left;
    return make_not(e);
}
ExprPtr conjunction(ExprPtr a, ExprPtr b)
{
    if (a->kind == ExprKind::Constant)
        return a->value ? b : a;
    if (b->kind == ExprKind::Constant)
        return b->value ? a : b;
    return make_binary(ExprKind::And, a, b);
}
ExprPtr disjunction(ExprPtr a, ExprPtr b)
{
    if (a->kind == ExprKind::Constant)
        return a->value ? a : b;
    if (b->kind == ExprKind::Constant)
        return b->value ? b : a;
    return make_binary(ExprKind::Or, a, b);
}

// Numeric constants are permitted only as a complete assignment or a
// comparison operand; signal vectors are ordered most-significant bit first.
unsigned long long parse_numeric_literal(const std::string& s)
{
    unsigned base     = 10;
    std::size_t start = 0;
    if (s.size() > 2 && s[0] == '#') {
        start = 2;
        switch (s[1]) {
        case 'B':
            base = 2;
            break;
        case 'O':
            base = 8;
            break;
        case 'D':
            base = 10;
            break;
        case 'H':
            base = 16;
            break;
        default:
            fail("Invalid radix: " + s);
        }
    }
    if (start == s.size())
        fail("Missing numeric digits");
    unsigned long long n = 0;
    for (std::size_t i = start; i < s.size(); ++i) {
        auto c         = s[i];
        unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99;
        // The historical parser accepts out-of-radix decimal digits (#B02
        // appears in PF_SRC); it accumulates these with the declared radix.
        if (digit > 15 || (digit >= 10 && base != 16))
            fail("Invalid numeric value: " + s);
        if (n > (~0ULL - digit) / base)
            fail("Numeric overflow: " + s);
        n = n * base + digit;
    }
    return n;
}
bool is_numeric_literal(const std::string& s)
{
    return !s.empty() && (s[0] == '#' || std::isdigit(static_cast<unsigned char>(s[0])));
}

class SynthesisParser
{
    Design& design;
    FrontendOptions options;
    std::map<std::string, std::vector<std::string>> groups;
    std::vector<Token> tokens;
    std::size_t pos  = 0;
    bool minimize    = true;
    unsigned nesting = 0;
    std::vector<ExprPtr> guards;
    std::map<std::string, std::size_t> assigned;
    std::map<std::string, ExprPtr> conditions;
    std::string tok(std::size_t n = 0) const { return pos + n < tokens.size() ? tokens[pos + n].text : ""; }
    bool take(const std::string& s)
    {
        if (tok() != s)
            return false;
        ++pos;
        return true;
    }
    void expect(const std::string& s)
    {
        if (!take(s))
            fail("Expected " + s + ", found " + tok());
    }
    std::string pop()
    {
        auto s = tok();
        if (s.empty())
            fail("Unexpected end of synthesis section");
        ++pos;
        return s;
    }
    Pin& pin(const std::string& name)
    {
        auto p = std::find_if(design.pins.begin(), design.pins.end(), [&](const Pin& p) { return p.name == name; });
        if (p == design.pins.end())
            fail("Undeclared signal: " + name);
        return *p;
    }
    std::vector<std::string> names(const std::string& name)
    {
        auto g = groups.find(name);
        return g == groups.end() ? expand_signal_names(name) : g->second;
    }
    bool assignment_at(std::size_t at) const
    {
        if (at >= tokens.size())
            return false;
        if (tokens[at].text == "/")
            ++at;
        if (at >= tokens.size() || tokens[at].text.empty() ||
            !(std::isalpha(static_cast<unsigned char>(tokens[at].text[0])) || tokens[at].text[0] == '_'))
            return false;
        ++at;
        if (at < tokens.size() && tokens[at].text == ".")
            at += 2;
        return at < tokens.size() && (tokens[at].text == "=" || tokens[at].text == ":=" || tokens[at].text == "*=");
    }
    std::string collect(const std::set<std::string>& stops, bool boundary = false)
    {
        std::string s;
        unsigned depth = 0;
        while (!tok().empty()) {
            if (depth == 0 && (stops.count(tok()) || (boundary && !s.empty() && assignment_at(pos))))
                break;
            auto t = pop();
            if (t == "(" || t == "{")
                ++depth;
            if (t == ")" || t == "}") {
                if (!depth)
                    fail("Unmatched " + t);
                --depth;
            }
            s += t + " ";
        }
        if (depth)
            fail("Unclosed expression");
        return trim(s);
    }
    std::string compact(std::string s)
    {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
        return s;
    }
    ExprPtr expression(std::string s, std::size_t bit = 0, std::size_t width = 1)
    {
        s = compact(s);
        if (is_numeric_literal(s) && s.find_first_of("*+/:()") == std::string::npos) {
            auto n = parse_numeric_literal(s);
            if (width < 64 && (n >> width))
                fail("Constant does not fit vector: " + s);
            return make_constant(width - bit <= 64 && ((n >> (width - bit - 1)) & 1));
        }
        s = select_vector_element(s, bit, width);
        // Copies are expanded as ASTs, so a copied expression remains grouped.
        std::map<std::string, ExprPtr> copies;
        for (std::size_t start = s.find('{'); start != std::string::npos; start = s.find('{')) {
            auto end = s.find('}', start);
            if (end == std::string::npos)
                fail("Unclosed equation copy");
            auto name     = s.substr(start + 1, end - start - 1);
            auto dot      = name.find('.');
            auto signal   = name.substr(0, dot);
            auto function = dot == std::string::npos ? "" : name.substr(dot + 1);
            function      = resolve_function(pin(signal), function, "=");
            auto a        = assigned.find(signal + "." + function);
            if (a == assigned.end())
                fail("Equation copy has no earlier definition: " + name);
            auto id    = "__COPY_" + std::to_string(copies.size());
            copies[id] = design.equations[a->second].expression;
            s.replace(start, end - start + 1, id);
        }
        auto without_indices = std::regex_replace(s, index_suffix_pattern(), "");
        if (std::regex_search(without_indices, bare_number_pattern()))
            fail("Numeric constants within Boolean expressions require VCC/GND");
        auto e                                  = parse_expression(s);
        std::function<ExprPtr(ExprPtr)> resolve = [&](ExprPtr v) -> ExprPtr {
            if (v->kind == ExprKind::Identifier) {
                auto c = copies.find(v->name);
                if (c != copies.end())
                    return c->second;
                auto cond = conditions.find(v->name);
                if (cond != conditions.end())
                    return cond->second;
                pin(v->name);
                return v;
            }
            if (v->kind == ExprKind::Not)
                return negate(resolve(v->left));
            if (v->left)
                return make_binary(v->kind, resolve(v->left), resolve(v->right));
            return v;
        };
        return resolve(e);
    }
    std::vector<std::string> selector(std::string s)
    {
        s = compact(s);
        if (s.size() > 1 && s.front() == '(' && s.back() == ')')
            s = s.substr(1, s.size() - 2);
        std::vector<std::string> out;
        std::size_t first = 0;
        unsigned depth    = 0;
        for (std::size_t i = 0; i <= s.size(); ++i) {
            if (i < s.size() && s[i] == '[')
                ++depth;
            if (i < s.size() && s[i] == ']')
                --depth;
            if (i == s.size() || (s[i] == ',' && !depth)) {
                auto part = s.substr(first, i - first);
                bool inv  = !part.empty() && part[0] == '/';
                if (inv)
                    part.erase(0, 1);
                for (auto name : names(part)) {
                    pin(name);
                    out.push_back(inv ? "/" + name : name);
                }
                first = i + 1;
            }
        }
        if (out.size() > 64)
            fail("CASE/comparison selector exceeds 64 bits");
        return out;
    }
    ExprPtr equal(const std::vector<std::string>& bits, unsigned long long n)
    {
        if (bits.size() < 64 && (n >> bits.size()))
            fail("CASE/comparison value exceeds selector width");
        auto e = make_constant(true);
        for (std::size_t i = 0; i < bits.size(); ++i) {
            auto b = expression(bits[i]);
            if (!((n >> (bits.size() - i - 1)) & 1))
                b = negate(b);
            e = conjunction(b, e);
        }
        return e;
    }
    ExprPtr interval(const std::vector<std::string>& bits, unsigned long long lo, unsigned long long hi,
                     unsigned branch_index)
    {
        if (bits.size() > 32)
            fail("CASE range selector exceeds original 32-bit limit");
        if (hi >> bits.size())
            fail("CASE range exceeds selector width");
        // Prepend lower and upper boundary products to the expression.
        // Compatibility conversion combines the low 16 bits of the value
        // with the branch index in the high word; normal mode uses the value.
        auto binary = [&](unsigned long long value) {
            auto n = options.dos_compatibility
                         ? (static_cast<unsigned>(branch_index & 0xffff) << 16) | static_cast<unsigned>(value & 0xffff)
                         : static_cast<unsigned>(value);
            std::string out(32, '0');
            for (unsigned i = 0; i < 32; ++i)
                if (n & (1u << (31 - i)))
                    out[i] = '1';
            return out;
        };
        auto low = binary(lo), high = binary(hi);
        auto result = make_constant(false);
        auto emit   = [&](const std::string& pattern) {
            auto term = make_constant(true);
            for (std::size_t i = 0; i < bits.size(); ++i) {
                auto c = pattern[32 - bits.size() + i];
                if (c == '-')
                    continue;
                auto lit = expression(bits[i]);
                if (c == '0')
                    lit = negate(lit);
                term = conjunction(lit, term);
            }
            result = disjunction(term, result);
        };
        unsigned common = 0;
        while (common < 32 && low[common] == high[common])
            ++common;
        bool done = false;
        while (common < 32 && low[common] != high[common] && !done) {
            auto pattern = low;
            for (unsigned j = 0; j < 32; ++j)
                if (low[j] == '0' && j > common && high.substr(0, j).find('1') != std::string::npos)
                    pattern[j] = '-';
            int j      = 31;
            bool found = false;
            for (; j >= 0; --j) {
                if (j < static_cast<int>(common)) {
                    done = true;
                    break;
                }
                if (low[j] == '1') {
                    found  = true;
                    low[j] = '0';
                    --j;
                    break;
                }
            }
            if (found) {
                for (; j >= 0; --j) {
                    if (low[j] == '0') {
                        low[j] = '1';
                        break;
                    }
                    low[j] = '0';
                }
            } else {
                auto first = high.find('1');
                if (first != std::string::npos)
                    low[first] = '1';
            }
            emit(pattern);
        }
        auto pattern = high;
        bool extra   = false;
        for (unsigned j = 0; j < 32; ++j)
            if (low[j] != high[j]) {
                if (high[j] != '1')
                    fail("Original CASE range conversion is not representable");
                pattern[j] = '-';
                if (high.find('0', j + 1) != std::string::npos)
                    extra = true;
            }
        emit(pattern);
        for (unsigned j = 0; j < 32 && extra; ++j)
            if (low[j] != high[j]) {
                pattern = high;
                for (unsigned k = 0; k < j; ++k)
                    if (low[k] != high[k])
                        pattern[k] = '-';
                if (high.find('0', j + 1) == std::string::npos) {
                    pattern[j] = '-';
                    extra      = false;
                } else
                    pattern[j] = '0';
                for (unsigned k = j + 1; k < 32; ++k)
                    pattern[k] = '-';
                emit(pattern);
            }
        return result;
    }
    ExprPtr condition(std::string s)
    {
        s = compact(s);
        while (s.size() > 1 && s.front() == '(' && s.back() == ')') {
            unsigned depth = 0;
            bool whole     = true;
            for (std::size_t i = 0; i + 1 < s.size(); ++i) {
                if (s[i] == '(')
                    ++depth;
                if (s[i] == ')')
                    --depth;
                if (!depth) {
                    whole = false;
                    break;
                }
            }
            if (!whole)
                break;
            s = s.substr(1, s.size() - 2);
        }
        auto eq = s.find('=');
        if (eq == std::string::npos)
            return expression(s);
        return equal(selector(s.substr(0, eq)), parse_numeric_literal(compact(s.substr(eq + 1))));
    }
    std::string resolve_function(Pin& p, std::string f, const std::string& op)
    {
        if (f == "CLKF")
            return "C";
        if (f == "RSTF")
            return "RE";
        if (f == "SETF")
            return "PR";
        if (f == "TRST")
            return "OE";
        // These PAL-era auxiliaries are parsed and may be copied with {},
        // but do not select MACH hardware equations.
        if (f == "T1" || f == "T2" || f == "TURBO" || f == "CMBF" || f == "PRLD")
            return f;
        if (p.storage_declared && ((op == "*=" && !p.latched) || (op == ":=" && !p.registered)))
            design.diagnostics.push_back("Ignoring " + op + " because " + p.name +
                                         " has an explicit storage declaration");
        if (!p.storage_declared && op == "*=") {
            p.latched    = true;
            p.registered = false;
        }
        if (!p.storage_declared && op == ":=") {
            p.registered = true;
            p.latched    = false;
        }
        if (f == "T" || f == "J" || f == "K" || f == "S" || f == "R") {
            if (!p.storage_declared) {
                p.registered = true;
                p.latched    = false;
            }
            return f;
        }
        if (!f.empty() && f != "D")
            fail("Unsupported sequential function: " + f);
        return p.latched ? "G" : p.registered ? "REG" : "COM";
    }
    ExprPtr guarded(ExprPtr value, ExprPtr fallback)
    {
        if (guards.empty())
            return conjunction(value, fallback);
        // Apply the innermost branch first and wrap each outer branch around
        // that result. Preserve these AND boundaries for ordered products.
        for (auto g = guards.rbegin(); g != guards.rend(); ++g)
            value = conjunction(value, *g);
        return value;
    }
    void add(const std::string& signal, const std::string& function, ExprPtr value, ExprPtr guard, bool negative,
             bool literal_assignment = false, bool source_assignment = false)
    {
        auto key = signal + "." + function;
        auto old = assigned.find(key);
        if (old == assigned.end()) {
            Equation e;
            e.signal     = signal;
            e.function   = function;
            e.expression = guarded(value, guard);
            e.source_clauses.push_back(e.expression);
            e.inverted    = negative;
            e.minimize    = minimize;
            e.care        = options.conditional_default_off ? make_constant(true) : guard;
            assigned[key] = design.equations.size();
            design.equations.push_back(e);
        } else {
            auto& e = design.equations[old->second];
            if (e.inverted != negative) {
                if (!literal_assignment) {
                    auto term             = guarded(value, guard);
                    e.opposite_expression = e.opposite_expression ? disjunction(e.opposite_expression, term) : term;
                    e.opposite_care       = e.opposite_care ? disjunction(e.opposite_care, guard) : guard;
                    e.opposite_minimize   = minimize;
                    return;
                }
                value = negate(value);
            }
            // Only a control inherited through a primary copy is replaced.
            // Every other repeated assignment adds a term, including one that
            // copies its own target with braces.
            const bool replace = e.inherited;
            e.inherited        = false;
            auto clause        = guarded(value, guard);
            if (replace)
                e.source_clauses.clear();
            e.source_clauses.push_back(clause);
            // Prepend repeated contributions as value * guard, with a right-hand
            // chain of older branches.
            bool prepend = source_assignment || !guards.empty() || guard->kind != ExprKind::Constant || !guard->value;
            e.expression = replace   ? clause
                           : prepend ? disjunction(clause, e.expression)
                                     : disjunction(e.expression, clause);
            e.care       = disjunction(e.care, guard);
            e.minimize   = minimize;
        }
    }
    void assignment(ExprPtr guard)
    {
        bool negative = take("/");
        auto target   = pop();
        std::string f;
        if (take("."))
            f = pop();
        auto op = pop();
        if (op != "=" && op != ":=" && op != "*=")
            fail("Expected assignment after " + target);
        auto rhs = collect({"END", "ELSE", "IF", "CASE", "MINIMIZE_ON", "MINIMIZE_OFF", "STATE", "CONDITIONS"}, true);
        auto lhs = names(target);
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            bool inv  = negative;
            auto name = lhs[i];
            if (!name.empty() && name[0] == '/') {
                inv = !inv;
                name.erase(0, 1);
            }
            auto& p       = pin(name);
            auto function = resolve_function(p, f, op);
            auto value    = expression(rhs, i, lhs.size());
            if (is_numeric_literal(compact(rhs)) && value->kind == ExprKind::Constant) {
                inv   = inv != !value->value;
                value = make_constant(true);
            }
            add(name, function, value, guard, inv, is_numeric_literal(compact(rhs)), true);
            // A primary function copy inherits auxiliary controls. Explicit
            // controls later in the source replace copied values.
            if (f.empty() && compact(rhs).front() == '{' && compact(rhs).back() == '}') {
                auto copied = compact(select_vector_element(rhs, i, lhs.size()));
                auto source = copied.substr(1, copied.size() - 2);
                if (source.find('.') == std::string::npos)
                    for (auto ctl : {"C", "PR", "RE", "OE"}) {
                        auto found = assigned.find(source + "." + ctl);
                        if (found != assigned.end() && !assigned.count(name + "." + ctl)) {
                            auto e                     = design.equations[found->second];
                            e.signal                   = name;
                            e.inherited                = true;
                            assigned[name + "." + ctl] = design.equations.size();
                            design.equations.push_back(e);
                        }
                    }
            }
        }
    }
    void scoped_statement(ExprPtr predicate, ExprPtr guard)
    {
        guards.push_back(predicate);
        statement(conjunction(guard, predicate));
        guards.pop_back();
    }
    void statement(ExprPtr guard)
    {
        if (nesting >= 256)
            fail("Synthesis block nesting exceeds 256");
        struct Depth
        {
            unsigned& n;
            explicit Depth(unsigned& depth) : n(depth) { ++n; }
            ~Depth() { --n; }
        } depth(nesting);
        if (take("BEGIN")) {
            while (!take("END")) {
                if (tok().empty())
                    fail("Unclosed BEGIN");
                statement(guard);
            }
            return;
        }
        if (take("IF")) {
            auto c = condition(collect({"THEN"}));
            expect("THEN");
            if (tok() != "BEGIN")
                fail("IF THEN requires BEGIN ... END");
            scoped_statement(c, guard);
            if (take("ELSE")) {
                if (tok() != "BEGIN")
                    fail("ELSE requires BEGIN ... END");
                scoped_statement(negate(c), guard);
            }
            return;
        }
        if (take("CASE")) {
            auto bits = selector(collect({"BEGIN"}));
            expect("BEGIN");
            auto covered          = make_constant(false);
            unsigned branch_index = 0;
            while (!take("END")) {
                ExprPtr c;
                if (take("OTHERWISE"))
                    c = negate(covered);
                else {
                    auto first = parse_numeric_literal(pop()), last = first;
                    if (take(".."))
                        last = parse_numeric_literal(pop());
                    if (bits.size() > 32)
                        fail("CASE selector exceeds original 32-bit limit");
                    if ((first >> bits.size()) || (last >> bits.size()))
                        fail("CASE value exceeds selector width");
                    if (first == last) {
                        auto value =
                            options.dos_compatibility
                                ? ((static_cast<unsigned long long>(branch_index & 0xffff) << 16) | (first & 0xffff))
                                : first;
                        value &= (1ULL << bits.size()) - 1;
                        c = equal(bits, value);
                    } else
                        c = interval(bits, std::min(first, last), std::max(first, last), branch_index);
                    covered = disjunction(covered, c);
                }
                expect(":");
                if (tok() != "BEGIN")
                    fail("CASE branch requires BEGIN ... END");
                scoped_statement(c, guard);
                ++branch_index;
            }
            return;
        }
        if (take("MINIMIZE_OFF")) {
            minimize = false;
            return;
        }
        if (take("MINIMIZE_ON")) {
            minimize = true;
            return;
        }
        assignment(guard);
    }
    void state_segment()
    {
        expect("STATE");
        minimize   = true;
        bool mealy = true;
        if (take("MOORE_MACHINE"))
            mealy = false;
        else
            take("MEALY_MACHINE");
        struct Record
        {
            std::string name, function, op, rhs;
        };
        std::vector<Record> records;
        std::string default_branch, clock, startup, startup_outputs;
        std::string default_outputs, held_outputs;
        const std::set<std::string> directives = {"CONDITIONS", "DEFAULT_BRANCH", "DEFAULT_OUTPUT", "OUTPUT_HOLD"};
        while (!tok().empty() && tok() != "CONDITIONS") {
            if (take("DEFAULT_BRANCH")) {
                default_branch = pop();
                continue;
            }
            if (take("DEFAULT_OUTPUT")) {
                default_outputs = collect(directives, true);
                continue;
            }
            if (take("OUTPUT_HOLD")) {
                held_outputs = collect(directives, true);
                continue;
            }
            auto name = pop();
            std::string function;
            if (take("."))
                function = pop();
            auto op = pop();
            if (op != "=" && op != ":=")
                fail("Unsupported STATE directive: " + name);
            auto rhs = collect(directives, true);
            if (name == "CLKF" || name == "NCLKF") {
                clock = (name == "NCLKF" ? "/" : "") + rhs;
                continue;
            }
            if (name == "START_UP") {
                if (design.device.compare(0, 5, "MACH3") == 0 || design.device.compare(0, 5, "MACH4") == 0)
                    fail("START_UP is not supported on this device");
                if (function == "OUTF") {
                    if (op != ":=" || !startup_outputs.empty())
                        fail("Expected one START_UP.OUTF := output list");
                    startup_outputs = rhs;
                    if (startup_outputs.empty())
                        fail("Missing START_UP.OUTF output list");
                    continue;
                }
                if (!function.empty())
                    fail("Unsupported START_UP function: " + function);
                auto branch = tokenize(rhs);
                if (op != ":=" || branch.size() != 4 || branch[0].text != "POWER_UP" || branch[1].text != "->")
                    fail("START_UP requires one POWER_UP destination");
                if (!startup.empty())
                    fail("Duplicate START_UP equation");
                startup = branch[2].text;
                continue;
            }
            records.push_back({name, function, op, rhs});
        }
        if (take("CONDITIONS"))
            while (!tok().empty()) {
                auto name = pop();
                expect("=");
                auto rhs = collect({}, true);
                if (conditions.count(name))
                    fail("Duplicate CONDITION: " + name);
                conditions.emplace(name, expression(rhs));
            }
        std::map<std::string, Cube> codes;
        std::map<std::string, ExprPtr> predicates;
        std::vector<std::string> transition_order;
        for (const auto& r : records) {
            if (r.function.empty() && r.op == "=") {
                auto e     = expression(r.rhs);
                auto cubes = sum_of_products(e);
                if (cubes.size() != 1 || cubes[0].empty())
                    fail("STATE requires explicit literal encoding: " + r.name);
                if (!codes.emplace(r.name, cubes[0]).second)
                    fail("Duplicate STATE encoding: " + r.name);
                predicates[r.name] = e;
            }
            if (r.function.empty() && r.op == ":=") {
                // The original's E1020. A second equation used to be added
                // to the first as further branches.
                if (std::find(transition_order.begin(), transition_order.end(), r.name) != transition_order.end())
                    fail("E1020: Multiple state transition equations for the same state " + r.name);
                transition_order.push_back(r.name);
            }
        }
        // MACHEXP E2791: every explicitly encoded state needs a transition
        // equation, whether or not another state branches to it.
        for (const auto& r : records)
            if (r.function.empty() && r.op == "=" &&
                std::find(transition_order.begin(), transition_order.end(), r.name) == transition_order.end())
                fail("E2791: State " + r.name +
                     " has a state assignment equation but is missing a transition equation");
        // The term a global DEFAULT_BRANCH adds for a state: the state's code
        // under the complement of the conditions its equation covers. The
        // original writes the complement out as products. Within a product
        // the signals stand in the order in which the transition equations
        // first mention them, followed by the state code; the products are
        // listed with the latest-mentioned signal first. The order decides
        // the variable order of the minimizer and with it the product rows.
        std::vector<std::string> mention_order;
        for (const auto& r : records)
            if (r.function.empty() && r.op == ":=")
                for (const auto& token : tokenize(r.rhs)) {
                    if (token.text.empty() ||
                        !(std::isalpha(static_cast<unsigned char>(token.text[0])) || token.text[0] == '_'))
                        continue;
                    std::vector<std::string> names;
                    const auto condition = conditions.find(token.text);
                    if (condition != conditions.end())
                        names = expression_variables(condition->second);
                    else if (!codes.count(token.text))
                        names = {token.text};
                    for (const auto& name : names)
                        if (std::find(mention_order.begin(), mention_order.end(), name) == mention_order.end())
                            mention_order.push_back(name);
                }
        const auto default_branch_term = [&](const ExprPtr& active, const ExprPtr& covered,
                                             const std::vector<ExprPtr>& branches) -> ExprPtr {
            using Literal = std::pair<std::string, bool>;
            using Product = std::vector<Literal>;
            // The literals of a plain product; false for anything else.
            std::function<bool(const ExprPtr&, bool, Product&)> literals = [&](const ExprPtr& e, bool negated,
                                                                             Product& out) -> bool {
                if (e->kind == ExprKind::Identifier) {
                    out.push_back({e->name, !negated});
                    return true;
                }
                if (e->kind == ExprKind::Not)
                    return literals(e->left, !negated, out);
                return e->kind == ExprKind::And && !negated && literals(e->left, false, out) &&
                       literals(e->right, false, out);
            };
            Product state;
            bool plain = literals(active, false, state);
            std::vector<Product> complement{Product{}};
            for (const auto& branch : branches) {
                Product condition;
                plain = plain && literals(branch, false, condition);
                if (!plain)
                    break;
                // De Morgan: multiply by the sum of the complemented literals.
                std::vector<Product> next;
                for (const auto& base : complement)
                    for (const auto& literal : condition) {
                        const auto same = std::find_if(base.begin(), base.end(),
                                                       [&](const Literal& l) { return l.first == literal.first; });
                        if (same == base.end()) {
                            next.push_back(base);
                            next.back().push_back({literal.first, !literal.second});
                        } else if (same->second != literal.second)
                            next.push_back(base);
                    }
                complement = std::move(next);
            }
            // A condition that is not a plain product keeps the general form.
            if (!plain)
                return conjunction(active, negate(covered));
            const auto mentioned = [&](const std::string& name) {
                return std::find(mention_order.begin(), mention_order.end(), name) - mention_order.begin();
            };
            for (auto& product : complement)
                std::stable_sort(product.begin(), product.end(), [&](const Literal& a, const Literal& b) {
                    return mentioned(a.first) < mentioned(b.first);
                });
            std::stable_sort(complement.begin(), complement.end(), [&](const Product& a, const Product& b) {
                std::vector<std::ptrdiff_t> x, y;
                for (const auto& literal : a)
                    x.push_back(mentioned(literal.first));
                for (const auto& literal : b)
                    y.push_back(mentioned(literal.first));
                std::sort(x.rbegin(), x.rend());
                std::sort(y.rbegin(), y.rend());
                return x > y;
            });
            auto sum = make_constant(false);
            for (const auto& product : complement) {
                auto term = make_constant(true);
                for (const Product* part : {&product, static_cast<const Product*>(&state)})
                    for (const auto& literal : *part) {
                        auto signal = make_identifier(literal.first);
                        term        = conjunction(term, literal.second ? signal : negate(signal));
                    }
                sum = disjunction(sum, term);
            }
            return sum;
        };
        bool automatic = codes.empty();
        if (automatic) {
            if (transition_order.size() < 2)
                fail("Automatic STATE encoding requires at least two states");
            unsigned width = 0;
            while ((1ULL << width) < transition_order.size())
                ++width;
            if (width > 32)
                fail("Automatic STATE encoding exceeds 32 bits");
            for (unsigned bit = 0; bit < width; ++bit) {
                auto name = "_ST" + std::to_string(bit);
                if (std::any_of(design.pins.begin(), design.pins.end(), [&](const Pin& p) { return p.name == name; }))
                    fail("Automatic state-bit name is already declared: " + name);
                Pin p;
                p.name       = name;
                p.node       = true;
                p.registered = true;
                design.pins.push_back(p);
            }
            for (std::size_t index = 0; index < transition_order.size(); ++index) {
                Cube code;
                auto predicate = make_constant(true);
                for (unsigned bit = width; bit-- > 0;) {
                    auto name    = "_ST" + std::to_string(bit);
                    bool value   = (index >> bit) & 1;
                    code[name]   = value;
                    auto literal = make_identifier(name);
                    if (!value)
                        literal = negate(literal);
                    predicate = conjunction(literal, predicate);
                }
                if (!codes.emplace(transition_order[index], code).second)
                    fail("Duplicate STATE transition: " + transition_order[index]);
                predicates[transition_order[index]] = predicate;
            }
        }
        if (!startup.empty()) {
            auto initial = codes.find(startup);
            if (initial == codes.end())
                fail("Unknown START_UP destination: " + startup);
            // Compatibility mode ignores POWER_UP on MACH1/2.
            // Normal automatic encoding assigns zero to the requested state.
            // Explicit codes retain their encoding and constrain register phase.
            if (!options.dos_compatibility) {
                if (automatic) {
                    auto first = transition_order.front();
                    std::swap(codes[first], codes[startup]);
                    std::swap(predicates[first], predicates[startup]);
                } else {
                    // Explicit state codes cannot be exchanged. Constrain the
                    // physical register phases instead, exactly as for OUTF.
                    for (const auto& bit : initial->second)
                        design.requested_power_up.emplace(bit.first, bit.second);
                }
            }
        }
        const auto& first = codes.begin()->second;
        std::set<std::string> state_bits;
        for (const auto& b : first)
            state_bits.insert(b.first);
        for (const auto& entry : codes) {
            std::set<std::string> bits;
            for (const auto& b : entry.second)
                bits.insert(b.first);
            if (bits != state_bits)
                fail("Inconsistent STATE encoding width");
            for (const auto& other : codes)
                if (entry.first < other.first && entry.second == other.second)
                    fail("Duplicate STATE code");
        }
        // State expansion introduces positive storage equations. Earlier
        // positive D/latch/COM contributions remain additive; negative covers
        // and alternate T/JK/RS excitation are replaced by the new primary.
        auto clear_primary = [&](const std::string& name) {
            design.equations.erase(
                std::remove_if(
                    design.equations.begin(), design.equations.end(),
                    [&](const Equation& e) {
                        return e.signal == name &&
                               (e.function == "T" || e.function == "J" || e.function == "K" || e.function == "S" ||
                                e.function == "R" ||
                                (e.inverted && (e.function == "REG" || e.function == "G" || e.function == "COM")));
                    }),
                design.equations.end());
            assigned.clear();
            for (std::size_t i = 0; i < design.equations.size(); ++i)
                assigned[design.equations[i].signal + "." + design.equations[i].function] = i;
        };
        std::set<std::string> state_driven = state_bits;
        for (const auto& name : state_bits)
            clear_primary(name);
        std::map<std::string, ExprPtr> earlier_outputs;
        std::set<std::string> state_clocked;
        auto drive = [&](const Cube& values, ExprPtr guard) {
            for (const auto& value : values) {
                // Select the generated primary while retaining additive
                // same-mode positive equations and independent controls.
                const bool first_drive = state_driven.insert(value.first).second;
                if (first_drive)
                    clear_primary(value.first);
                auto& p       = pin(value.first);
                auto function = resolve_function(p, "", "=");
                if (first_drive) {
                    auto earlier = assigned.find(value.first + "." + function);
                    if (earlier != assigned.end()) {
                        auto& e                                      = design.equations[earlier->second];
                        earlier_outputs[e.signal + "." + e.function] = e.expression;
                        e.expression                                 = make_constant(false);
                        e.source_clauses.clear();
                    }
                }
                if (function != "REG" && function != "COM" && function != "G")
                    fail("Unsupported STATE output storage");
                auto old      = assigned.find(value.first + "." + function);
                auto previous = old == assigned.end() ? make_constant(false) : design.equations[old->second].expression;
                add(value.first, function, make_constant(value.second), guard, false);
                auto& generated = design.equations[assigned.at(value.first + "." + function)];
                if (!value.second)
                    generated.pairing_opposite_expression =
                        generated.pairing_opposite_expression
                            ? disjunction(generated.pairing_opposite_expression, guard)
                            : guard;
                if (automatic || state_bits.count(value.first))
                    generated.expression = disjunction(previous, value.second ? guard : make_constant(false));
                if (!clock.empty() && (p.registered || p.latched) && state_clocked.insert(value.first).second)
                    add(value.first, "C", expression(clock), make_constant(true), false);
            }
        };
        auto decode = [&](const std::string& s) -> Cube {
            // STATE output lists permit whitespace as well as explicit '*'.
            Cube values;
            bool inverted = false;
            for (const auto& token : tokenize(s)) {
                auto value = token.text;
                if (value.empty())
                    break;
                if (value == "/") {
                    if (inverted)
                        fail("Repeated output complement");
                    inverted = true;
                    continue;
                }
                if (value == "*" || value == "," || value == "(" || value == ")")
                    continue;
                for (const auto& name : names(value)) {
                    pin(name);
                    auto old = values.find(name);
                    if (old != values.end() && old->second == inverted)
                        fail("Conflicting STATE output values: " + name);
                    values[name] = !inverted;
                }
                inverted = false;
            }
            if (inverted)
                fail("Missing complemented STATE output");
            return values;
        };
        auto defaults = decode(default_outputs), holds = decode(held_outputs);
        for (const auto& hold : holds)
            if (!hold.second)
                fail("OUTPUT_HOLD requires uncomplemented output names");
        std::map<std::string, std::map<std::string, ExprPtr>> output_coverage;
        auto output_drive = [&](const std::string& state, const Cube& values, ExprPtr condition, ExprPtr guard) {
            drive(values, guard);
            for (const auto& value : values) {
                auto& coverage = output_coverage[state][value.first];
                coverage       = coverage ? disjunction(coverage, condition) : condition;
            }
        };
        for (const auto& r : records) {
            if (r.op == "=" && r.function.empty())
                continue;
            if (!codes.count(r.name))
                fail("Missing STATE encoding: " + r.name);
            auto active = predicates.at(r.name);
            if (r.function == "OUTF" && !mealy) {
                auto values = decode(r.rhs);
                if (r.op == ":=")
                    for (const auto& value : values) {
                        auto& output = pin(value.first);
                        if (!output.storage_declared) {
                            output.registered = true;
                            output.latched    = false;
                        }
                    }
                output_drive(r.name, values, make_constant(true), active);
                continue;
            }
            if (r.function != "OUTF" && r.op != ":=")
                fail("Unsupported STATE equation: " + r.name);
            auto branches      = tokenize(r.rhs);
            std::size_t at     = 0;
            auto covered       = make_constant(false);
            bool local_default = false;
            std::vector<ExprPtr> branch_conditions;
            while (!branches[at].text.empty()) {
                ExprPtr cond;
                if (branches[at].text == "+->") {
                    ++at;
                    cond          = negate(covered);
                    local_default = true;
                } else {
                    if (branches[at].text == "+")
                        ++at;
                    std::string text;
                    std::vector<std::string> words;
                    while (!branches[at].text.empty() && branches[at].text != "->") {
                        words.push_back(branches[at].text);
                        text += branches[at++].text;
                    }
                    if (branches[at].text != "->")
                        fail("Expected STATE transition arrow");
                    ++at;
                    // The original takes a single signal or CONDITIONS name
                    // here, optionally complemented; an expression is P88.
                    const std::size_t name = !words.empty() && words[0] == "/" ? 1 : 0;
                    if (words.size() != name + 1)
                        fail("P88: Unexpected reserved word or operator " +
                             (words.size() > name + 1 ? words[name + 1] : std::string("->")));
                    cond = expression(text);
                    // E1250/E1260: the conditions of one equation must exclude
                    // each other, whatever their destinations are.
                    for (const auto& earlier : branch_conditions)
                        if (!sum_of_products(conjunction(earlier, cond)).empty())
                            fail(r.function == "OUTF"
                                     ? "E1260: Conflicting conditions in output equation of state " + r.name
                                     : "E1250: Conflicting conditions in transition equation of state " + r.name);
                    branch_conditions.push_back(cond);
                    covered = disjunction(covered, cond);
                }
                std::string target;
                while (!branches[at].text.empty() && branches[at].text != "+" && branches[at].text != "+->")
                    target += branches[at++].text;
                if (target.empty())
                    fail("Missing STATE branch target");
                auto guard = (automatic || (r.function.empty() && !local_default)) ? conjunction(cond, active)
                                                                                   : conjunction(active, cond);
                if (r.function == "OUTF") {
                    auto values = decode(target);
                    if (r.op == ":=")
                        for (const auto& value : values) {
                            auto& output = pin(value.first);
                            if (!output.storage_declared) {
                                output.registered = true;
                                output.latched    = false;
                            }
                        }
                    output_drive(r.name, values, cond, guard);
                } else {
                    if (!codes.count(target))
                        fail("Unknown destination STATE: " + target);
                    drive(codes.at(target), guard);
                }
            }
            // The global default belongs to transition equations. An output
            // equation's conditions say nothing about the next state.
            if (r.function.empty() && !local_default && !default_branch.empty()) {
                auto target = default_branch;
                if (target == "HOLD_STATE")
                    target = r.name;
                else if (target == "NEXT_STATE") {
                    auto it = std::find(transition_order.begin(), transition_order.end(), r.name);
                    ++it;
                    if (it == transition_order.end())
                        it = transition_order.begin();
                    target = *it;
                }
                if (!codes.count(target))
                    fail("Unknown default STATE: " + target);
                drive(codes.at(target), default_branch_term(active, covered, branch_conditions));
            }
        }
        for (const auto& name : transition_order) {
            auto active = predicates.at(name);
            for (const auto& value : defaults) {
                auto coverage = output_coverage[name][value.first];
                auto guard    = coverage ? conjunction(active, negate(coverage)) : active;
                drive(Cube{{value.first, value.second}}, guard);
            }
            for (const auto& value : holds) {
                if (defaults.count(value.first))
                    fail("OUTPUT_HOLD conflicts with DEFAULT_OUTPUT: " + value.first);
                auto coverage = output_coverage[name][value.first];
                auto guard    = coverage ? conjunction(active, negate(coverage)) : active;
                auto& p       = pin(value.first);
                auto function = resolve_function(p, "", "=");
                auto key      = value.first + "." + function;
                auto old      = assigned.find(key);
                auto old_care = old == assigned.end() ? make_constant(false) : design.equations[old->second].care;
                add(value.first, function, conjunction(guard, make_identifier(value.first)), make_constant(true),
                    false);
                if (!options.conditional_default_off)
                    design.equations[assigned.at(key)].care = disjunction(old_care, guard);
                if (!clock.empty() && (p.registered || p.latched) && state_clocked.insert(value.first).second)
                    add(value.first, "C", expression(clock), make_constant(true), false);
            }
        }
        for (const auto& earlier : earlier_outputs) {
            auto& e          = design.equations.at(assigned.at(earlier.first));
            e.expression     = disjunction(earlier.second, e.expression);
            e.source_clauses = {e.expression};
        }
        if (!startup_outputs.empty()) {
            Cube initial;
            std::vector<std::pair<ExprPtr, Cube>> asynchronous;
            if (startup_outputs.find("->") == std::string::npos)
                initial = decode(startup_outputs);
            else {
                auto branches   = tokenize(startup_outputs);
                std::size_t at  = 0;
                bool power_seen = false;
                while (!branches[at].text.empty()) {
                    if (branches[at].text == "+")
                        ++at;
                    std::string condition;
                    while (!branches[at].text.empty() && branches[at].text != "->")
                        condition += branches[at++].text;
                    if (branches[at].text != "->")
                        fail("Expected START_UP.OUTF branch arrow");
                    ++at;
                    std::string outputs;
                    while (!branches[at].text.empty() && branches[at].text != "+" && branches[at].text != "+->")
                        outputs += branches[at++].text + " ";
                    if (outputs.empty())
                        fail("Missing START_UP.OUTF branch outputs");
                    auto values = decode(outputs);
                    if (condition == "POWER_UP") {
                        if (power_seen)
                            fail("Duplicate POWER_UP branch");
                        power_seen = true;
                        initial    = std::move(values);
                    } else {
                        asynchronous.emplace_back(expression(condition), std::move(values));
                    }
                }
            }
            if (options.dos_compatibility)
                design.diagnostics.push_back("START_UP.OUTF ignored to reproduce original DOS initialization");
            else {
                for (const auto& value : initial)
                    if (pin(value.first).registered && assigned.count(value.first + ".REG"))
                        design.requested_power_up.emplace(value.first, value.second);
                for (const auto& value : initial)
                    if (!design.requested_power_up.count(value.first)) {
                        if (!assigned.count(value.first + ".COM"))
                            fail("Corrected START_UP.OUTF requires storage or a combinatorial startup proof: " +
                                 value.first);
                        // A COM output has no independent initialization bit.
                        // Accept the requirement only when its actual equation
                        // is fixed to that value at register power-up, for every
                        // possible input value. Lock the observed register phases
                        // so optimizer choices cannot invalidate this proof.
                        std::map<std::string, bool> phases;
                        std::set<std::string> visiting;
                        std::function<ExprPtr(const ExprPtr&)> initial_expression;
                        initial_expression = [&](const ExprPtr& e) -> ExprPtr {
                            if (e->kind == ExprKind::Constant)
                                return e;
                            if (e->kind == ExprKind::Identifier) {
                                auto stored = assigned.find(e->name + ".REG");
                                if (stored != assigned.end()) {
                                    auto requested   = design.requested_power_up.find(e->name);
                                    const bool phase = requested == design.requested_power_up.end()
                                                           ? design.equations[stored->second].inverted
                                                           : requested->second;
                                    phases[e->name]  = phase;
                                    return make_constant(phase);
                                }
                                auto combinatorial = assigned.find(e->name + ".COM");
                                if (combinatorial == assigned.end())
                                    return e;
                                if (!visiting.insert(e->name).second)
                                    fail("Combinatorial cycle in START_UP.OUTF: " + e->name);
                                const auto& equation = design.equations[combinatorial->second];
                                if (equation.care &&
                                    !sum_of_products(negate(initial_expression(equation.care))).empty())
                                    fail("START_UP.OUTF depends on unspecified combinatorial values: " + e->name);
                                auto result = initial_expression(equation.expression);
                                if (equation.inverted)
                                    result = negate(result);
                                visiting.erase(e->name);
                                return result;
                            }
                            if (e->kind == ExprKind::Not)
                                return negate(initial_expression(e->left));
                            return make_binary(e->kind, initial_expression(e->left), initial_expression(e->right));
                        };
                        auto actual = initial_expression(make_identifier(value.first));
                        if (!sum_of_products(value.second ? negate(actual) : actual).empty())
                            fail("Combinatorial START_UP.OUTF is not guaranteed by the startup state: " + value.first);
                        design.requested_power_up.insert(phases.begin(), phases.end());
                    }
            }
            if (!options.dos_compatibility)
                for (const auto& branch : asynchronous)
                    for (const auto& value : branch.second) {
                        auto primary = assigned.find(value.first + ".REG");
                        if (!pin(value.first).registered || primary == assigned.end())
                            fail("Conditional START_UP.OUTF requires registered output: " + value.first);
                        // Legacy asynchronous controls act before the output XOR.
                        // Constrain its phase even without an explicit POWER_UP
                        // branch, so later polarity selection cannot exchange the
                        // requested logical reset/preset values.
                        auto phase =
                            design.requested_power_up.emplace(value.first, design.equations[primary->second].inverted)
                                .first->second;
                        add(value.first, value.second == phase ? "RE" : "PR", branch.first, make_constant(true), false);
                    }
        }
        design.simulation_symbols.insert(predicates.begin(), predicates.end());
        design.simulation_symbols.insert(conditions.begin(), conditions.end());
    }

   public:
    SynthesisParser(Design& d, FrontendOptions o, std::map<std::string, std::vector<std::string>> g)
        : design(d), options(o), groups(std::move(g))
    {
    }
    void run(const std::string& s)
    {
        tokens = tokenize(s);
        while (!tok().empty()) {
            if (tok() == "STATE") {
                state_segment();
                break;
            }
            if (tok() == "CONDITIONS")
                fail("CONDITIONS requires a STATE segment");
            statement(make_constant(true));
        }
        // The first primary excitation family selects the hardware
        // function. Later families remain available to earlier brace-copy
        // expansion, but do not contribute to the selected equation.
        auto family = [](const std::string& function) {
            if (function == "J" || function == "K")
                return std::string("JK");
            if (function == "S" || function == "R")
                return std::string("RS");
            if (function == "REG" || function == "COM" || function == "G" || function == "T")
                return function;
            return std::string();
        };
        std::map<std::string, std::string> selected;
        for (const auto& e : design.equations)
            if (!family(e.function).empty())
                selected.emplace(e.signal, family(e.function));
        design.equations.erase(std::remove_if(design.equations.begin(), design.equations.end(),
                                              [&](const Equation& e) {
                                                  return !family(e.function).empty() &&
                                                         family(e.function) != selected.at(e.signal);
                                              }),
                               design.equations.end());
        assigned.clear();
        for (std::size_t i = 0; i < design.equations.size(); ++i)
            assigned[design.equations[i].signal + "." + design.equations[i].function] = i;
        for (auto& p : design.pins)
            if (selected.count(p.name)) {
                auto function = selected.at(p.name);
                if (function == "JK" || function == "RS") {
                    if (p.storage_declared)
                        continue;
                    function = "REG";
                }
                p.registered = function == "REG" || function == "T";
                p.latched    = function == "G";
            }
        // JK and RS excitation both lower to J*!Q + !K*Q.
        // As a result, simultaneous RS inputs toggle the stored value.
        std::vector<Equation> lowered;
        for (const auto& p : design.pins) {
            bool jk = assigned.count(p.name + ".J") || assigned.count(p.name + ".K");
            bool rs = assigned.count(p.name + ".S") || assigned.count(p.name + ".R");
            if (!jk && !rs)
                continue;
            if (jk && rs)
                fail("Mixed JK/RS equations: " + p.name);
            auto j = assigned.find(p.name + (jk ? ".J" : ".S")), k = assigned.find(p.name + (jk ? ".K" : ".R"));
            if (j == assigned.end() || k == assigned.end())
                fail("Both excitation equations are required: " + p.name);
            if (assigned.count(p.name + ".REG") || assigned.count(p.name + ".T") || assigned.count(p.name + ".COM"))
                fail("Mixed register excitation modes: " + p.name);
            auto a = design.equations[j->second], b = design.equations[k->second];
            // Keep the first excitation polarity when combining the equations,
            // even if a later contribution has the opposite polarity.
            a.opposite_expression.reset();
            a.opposite_care.reset();
            b.opposite_expression.reset();
            b.opposite_care.reset();
            if (a.inverted)
                a.expression = negate(a.expression);
            if (b.inverted)
                b.expression = negate(b.expression);
            a.inverted          = false;
            a.excitation_origin = jk ? "JK" : "RS";
            auto feedback       = make_identifier(p.name);
            a.function          = p.latched ? "G" : p.registered ? "REG" : "COM";
            a.expression =
                disjunction(conjunction(negate(b.expression), feedback), conjunction(a.expression, negate(feedback)));
            a.source_clauses = {a.expression};
            a.care           = conjunction(a.care, b.care);
            a.minimize       = a.minimize || b.minimize;
            lowered.push_back(a);
        }
        if (!lowered.empty()) {
            design.equations.erase(std::remove_if(design.equations.begin(), design.equations.end(),
                                                  [](const Equation& e) {
                                                      return e.function == "J" || e.function == "K" ||
                                                             e.function == "S" || e.function == "R";
                                                  }),
                                   design.equations.end());
            design.equations.insert(design.equations.end(), lowered.begin(), lowered.end());
            assigned.clear();
            for (std::size_t i = 0; i < design.equations.size(); ++i)
                assigned[design.equations[i].signal + "." + design.equations[i].function] = i;
        }
        design.equations.erase(
            std::remove_if(
                design.equations.begin(), design.equations.end(),
                [&](const Equation& e) {
                    const bool auxiliary = e.function == "T1" || e.function == "T2" || e.function == "TURBO" ||
                                           e.function == "CMBF" || e.function == "PRLD";
                    if (auxiliary)
                        design.diagnostics.push_back("No MACH hardware equation for " + e.signal + "." + e.function +
                                                     "; retained equation copies only, matching MACHMIN");
                    return auxiliary;
                }),
            design.equations.end());
    }
};

}  // namespace

// Pairing rules that follow the parse: the transfer equation of a declared
// input pairing, output pairs given on either name, fixed locations of
// pairs, and the automatic input and output pairing of MACHBPP.
void resolve_pairing(Design& design, const FrontendOptions& options)
{
    std::map<std::string, Pin*> pin_index;
    for (auto& p : design.pins)
        pin_index[p.name] = &p;
    // MACHBPP treats a PAIR declared on a pin as an input pairing: the node
    // must be a register or latch whose equation transfers the pad.
    //  - No equation for the node: generate [/]node = [/]pin with the node's
    //    declared storage type (warning B1062). Each side is complemented
    //    exactly when its declaration is, so the register stores the pad's
    //    active-high sense.
    //  - A transfer equation on a node without storage: error B615.
    //  - Any other D or latch equation: error B613, preceded by the B1062
    //    warning when the equation does not read the pad at all.
    // A toggle or JK/RS excitation is left to the fitter.
    {
        const auto reads = [](const ExprPtr& expression, const std::string& name) {
            std::function<bool(const ExprPtr&)> visit = [&](const ExprPtr& e) {
                return e && ((e->kind == ExprKind::Identifier && e->name == name) || visit(e->left) || visit(e->right));
            };
            return visit(expression);
        };
        for (const auto& pad : design.pins) {
            if (pad.node || pad.pair.empty())
                continue;
            const auto node = pin_index.find(pad.pair);
            if (node == pin_index.end() || !node->second->node)
                continue;  // Reported by the pair validation below.
            const auto equation =
                std::find_if(design.equations.begin(), design.equations.end(), [&](const Equation& e) {
                    return e.signal == pad.pair &&
                           (e.function == "REG" || e.function == "T" || e.function == "G" || e.function == "COM");
                });
            const bool stored = node->second->registered || node->second->latched;
            const auto b615   = "B615: Node " + pad.pair + " input-paired with pin " + pad.name +
                              " does not have REG/LAT attribute and the transfer equation is not of the form "
                              "[/]node := [/]pin or [/]node *= [/]pin.";
            const auto b1062 = std::string("B1062: Canonical equation ") + (node->second->inverted ? "/" : "") +
                               pad.pair + " (node) = " + (pad.inverted ? "/" : "") + pad.name +
                               " (pin) generated from PAIRing.";
            if (equation != design.equations.end()) {
                if (equation->function == "T" || !equation->excitation_origin.empty())
                    continue;
                auto literal = equation->expression;
                if (literal->kind == ExprKind::Not)
                    literal = literal->left;
                if (literal->kind == ExprKind::Identifier && literal->name == pad.name &&
                    !equation->opposite_expression) {
                    if (!stored)
                        fail(b615);
                    continue;
                }
                if (!reads(equation->expression, pad.name))
                    design.diagnostics.push_back(b1062);
                if (!stored)
                    fail(b615);
                fail("B613: Input-paired node " + pad.pair + " has an equation not of the form: [/]node = [/]pin for " +
                     "the input-paired pin " + pad.name + ".");
            }
            design.diagnostics.push_back(b1062);
            if (!stored)
                fail(b615);
            Equation transfer;
            transfer.signal     = pad.pair;
            transfer.function   = node->second->latched ? "G" : "REG";
            transfer.expression = make_identifier(pad.name);
            if (pad.inverted)
                transfer.expression = make_not(transfer.expression);
            transfer.inverted = node->second->inverted;
            transfer.source_clauses.push_back(transfer.expression);
            transfer.care = make_constant(true);
            design.equations.push_back(std::move(transfer));
            node->second->generated_transfer = true;
            // "Generating CLKF equation for INPUT PAIRING": on MACH2 a clocked
            // pad passes its clock on to the generated register. MACH4 leaves
            // the register on its default clock.
            const auto has_clock = [&](const std::string& name) {
                return std::find_if(design.equations.begin(), design.equations.end(),
                                    [&](const Equation& e) { return e.signal == name && e.function == "C"; });
            };
            const auto pad_clock = has_clock(pad.name);
            if (design.device.compare(0, 5, "MACH2") == 0 && pad_clock != design.equations.end() &&
                has_clock(pad.pair) == design.equations.end()) {
                auto clock   = *pad_clock;
                clock.signal = pad.pair;
                design.equations.push_back(std::move(clock));
            }
        }
    }
    auto normalize_fixed_output_pair = [&](Pin& node, Pin& pad) {
        if (!node.number && !pad.number)
            return true;
        const bool mach1   = design.device.compare(0, 5, "MACH1") == 0;
        const bool mach2   = design.device.compare(0, 5, "MACH2") == 0;
        const auto& layout = mach_device_layout(design.device);
        if (!mach1 && !mach2) {
            // MACH3/4 automatic pairing never constrains a fixed node.
            // A fixed pad with a floating node uses its default cell.
            if (node.number)
                return false;
            auto io = std::find(layout.io_pins.begin(), layout.io_pins.end(), pad.number);
            if (io == layout.io_pins.end())
                return false;
            const unsigned stride = design.device == "MACH355" ? 1 : 2;
            node.number           = 2 + stride * unsigned(io - layout.io_pins.begin());
            return true;
        }
        const unsigned stride = mach2 ? 2 : 1;
        if (node.number) {
            if (node.number < 2 || (node.number - 2) % stride)
                return false;
            const auto index = (node.number - 2) / stride;
            if (index >= layout.io_pins.size())
                return false;
            const auto physical = layout.io_pins[index];
            if (pad.number && pad.number != physical)
                return false;
            pad.number = physical;
        } else {
            auto physical = std::find(layout.io_pins.begin(), layout.io_pins.end(), pad.number);
            if (physical == layout.io_pins.end())
                return false;
            node.number = 2 + stride * unsigned(physical - layout.io_pins.begin());
        }
        return true;
    };
    auto fixed_modern_pair_routes = [&](const Pin& node, const Pin& pad) {
        const bool modern = design.device == "MACH355" || design.device.compare(0, 5, "MACH4") == 0;
        if (!modern || node.number < 2 || !pad.number)
            return true;
        const auto& layout = mach_device_layout(design.device);
        const auto io      = std::find(layout.io_pins.begin(), layout.io_pins.end(), pad.number);
        const auto cell    = node.number - 2;
        if (io == layout.io_pins.end() || cell >= layout.blocks * layout.cells)
            return true;  // Not a macrocell/I-O pair; rejected later by the fitter.
        const unsigned ios   = unsigned(layout.io_pins.size()) / layout.blocks;
        const unsigned index = unsigned(io - layout.io_pins.begin());
        if (cell / layout.cells != index / ios)
            return false;
        const unsigned local = cell % layout.cells, pin = index % ios;
        // Sixteen I/O pins per block reach the cells of their own half;
        // eight reach a window of eight cells starting at twice their index.
        return ios == 16 ? local / 8 == pin / 8 : (local + 16 - 2 * pin) % 16 < 8;
    };
    std::set<std::string> paired;
    for (auto& p : design.pins)
        if (!p.pair.empty()) {
            auto other = pin_index.find(p.pair);
            if (other == pin_index.end() || other->second->node == p.node)
                fail("PAIR must connect a declared pin and node: " + p.name);
            if (p.node && p.pair_kind != "IPAIR" && (design.device[4] == '1' || design.device[4] == '2') &&
                !normalize_fixed_output_pair(p, *other->second)) {
                p.pair.clear();
                p.pair_kind.clear();
                continue;
            }
            // MACHBPP B602: on MACH3/4 a fixed node and its fixed pad must be
            // joined by the block's output switch matrix. Otherwise the pair
            // is discarded and both signals keep their own locations.
            if (p.node && p.pair_kind != "IPAIR" && !fixed_modern_pair_routes(p, *other->second)) {
                design.diagnostics.push_back("B602: Incorrectly paired node " + p.name +
                                             ". Output pairing of the above node discarded.");
                p.pair.clear();
                p.pair_kind.clear();
                continue;
            }
            if (paired.count(p.name) || paired.count(p.pair))
                fail("Signal occurs in multiple PAIR declarations");
            paired.insert(p.name);
            paired.insert(p.pair);
        }
    auto eq = [&](const std::string& name, const std::string& function) -> const Equation* {
        for (const auto& e : design.equations)
            if (e.signal == name && e.function == function)
                return &e;
        return nullptr;
    };
    auto primary = [&](const Pin& p) -> const Equation* {
        for (const auto& f : {"REG", "T", "G", "COM"})
            if (auto e = eq(p.name, f))
                return e;
        return nullptr;
    };
    // An explicit output pair may supply its excitation on either name.
    // Create the missing primary before comparing partners and inheriting
    // their controls.
    for (auto& node : design.pins)
        if (node.node && !node.pair.empty() && node.pair_kind != "IPAIR") {
            auto& pad      = *pin_index.at(node.pair);
            const auto* ne = primary(node);
            const auto* pe = primary(pad);
            if (bool(ne) == bool(pe))
                continue;
            auto copy              = ne ? *ne : *pe;
            auto& destination      = ne ? pad : node;
            copy.signal            = destination.name;
            destination.registered = copy.function == "REG" || copy.function == "T";
            destination.latched    = copy.function == "G";
            design.equations.push_back(std::move(copy));
        }
    auto equivalent = [&](const Equation* a, const Equation* b, bool register_phase = false) {
        if (!a || !b)
            return a == b;
        if ((a->inverted != b->inverted && !register_phase) || a->function != b->function)
            return false;
        auto lhs = sum_of_products(a->expression), rhs = sum_of_products(b->expression);
        std::sort(lhs.begin(), lhs.end());
        std::sort(rhs.begin(), rhs.end());
        return lhs == rhs;
    };
    auto opposite_pair = [&](const Equation* a, const Equation* b) {
        if (!options.dos_compatibility || !a || !b || a->function != b->function)
            return false;
        auto opposite = [](const Equation* e) {
            return e->pairing_opposite_expression ? e->pairing_opposite_expression : e->opposite_expression;
        };
        auto equal_cover = [](const ExprPtr& lhs, const ExprPtr& rhs) {
            if (!lhs || !rhs)
                return false;
            auto left = sum_of_products(lhs), right = sum_of_products(rhs);
            std::sort(left.begin(), left.end());
            std::sort(right.begin(), right.end());
            return left == right;
        };
        return equal_cover(a->expression, opposite(b)) || equal_cover(opposite(a), b->expression) ||
               equal_cover(opposite(a), opposite(b));
    };
    // BPP removes a mismatched explicit output pair after physical fixed-pin
    // normalization. Retain the resulting numbers: conflicting fixed cells
    // remain a fitting error even though the logical pair has been deleted.
    for (auto& declared_pair : design.pins)
        if (!declared_pair.pair.empty() && declared_pair.pair_kind != "IPAIR" && declared_pair.node) {
            auto& partner = *pin_index.at(declared_pair.pair);
            const auto* a = primary(declared_pair);
            const auto* b = primary(partner);
            if (!a || !b)
                continue;
            auto lhs = sum_of_products(a->expression), rhs = sum_of_products(b->expression);
            std::sort(lhs.begin(), lhs.end());
            std::sort(rhs.begin(), rhs.end());
            bool compatible = lhs == rhs && a->function == b->function;
            lhs             = sum_of_products(a->care ? a->care : make_constant(true));
            rhs             = sum_of_products(b->care ? b->care : make_constant(true));
            std::sort(lhs.begin(), lhs.end());
            std::sort(rhs.begin(), rhs.end());
            compatible = compatible && lhs == rhs;
            for (const auto* function : {"C", "PR", "RE", "OE"}) {
                const auto* left  = eq(declared_pair.name, function);
                const auto* right = eq(partner.name, function);
                if (left && right && !equivalent(left, right))
                    compatible = false;
            }
            if (compatible) {
                // BPP copies controls supplied on only one half of an output
                // pair. A control specified on both halves must agree. The pad
                // equation's MINIMIZE policy wins when both equations exist.
                const auto& pad  = declared_pair.node ? partner : declared_pair;
                const auto& node = declared_pair.node ? declared_pair : partner;
                std::vector<Equation> inherited;
                for (const auto* function : {"C", "PR", "RE"}) {
                    const auto* pe = eq(pad.name, function);
                    const auto* ne = eq(node.name, function);
                    if (bool(pe) == bool(ne))
                        continue;
                    auto copy   = pe ? *pe : *ne;
                    copy.signal = pe ? node.name : pad.name;
                    inherited.push_back(std::move(copy));
                }
                design.equations.insert(design.equations.end(), inherited.begin(), inherited.end());
                for (auto& equation : design.equations)
                    if (equation.signal == node.name) {
                        if (const auto* pe = eq(pad.name, equation.function)) {
                            equation.minimize          = pe->minimize;
                            equation.opposite_minimize = pe->opposite_minimize;
                        }
                    }
                continue;
            }
            design.diagnostics.push_back("PAIR declaration deleted: equations, controls or storage types differ for " +
                                         declared_pair.name + " and " + partner.name);
            // Original does not reconsider either endpoint for automatic pairing
            // during this BPP pass (ZD435D would otherwise become an input pair).
            declared_pair.pair.clear();
            declared_pair.pair_kind.clear();
        }
    // Original modern fixed-node pairing has two linked defects. The
    // device helper compares a relative cell to an absolute node base,
    // and relocation happens before the explicit-pair reservation check.
    // Reproduce both only in compatibility mode (see paired-relocation.md).
    const bool modern_pairing = design.device == "MACH355" || design.device.compare(0, 5, "MACH4") == 0;
    if (options.dos_compatibility && options.automatic_output_pairing && modern_pairing) {
        const auto explicit_pairs = paired;
        const auto& layout        = mach_device_layout(design.device);
        const unsigned ios        = unsigned(layout.io_pins.size()) / layout.blocks;
        std::vector<Pin*> order;
        std::set<std::string> included;
        for (const auto& equation : design.equations)
            if (primary(*pin_index.at(equation.signal)) == &equation && included.insert(equation.signal).second)
                order.push_back(pin_index.at(equation.signal));
        std::set<unsigned> occupied;
        for (const auto& pin : design.pins)
            if (!pin.node && pin.number)
                occupied.insert(pin.number);
        for (std::size_t i = 0; i < order.size(); ++i)
            for (std::size_t j = i + 1; j < order.size(); ++j) {
                auto* node = order[i]->node ? order[i] : order[j];
                auto* pad  = order[i]->node ? order[j] : order[i];
                if (!node->node || pad->node || node->number < 2 || pad->number || explicit_pairs.count(pad->name))
                    continue;
                const auto* expression = primary(*node);
                if (!(equivalent(expression, primary(*pad), expression->function == "REG") ||
                      opposite_pair(expression, primary(*pad))) ||
                    !equivalent(eq(node->name, "C"), eq(pad->name, "C")) ||
                    !equivalent(eq(node->name, "PR"), eq(pad->name, "PR")) ||
                    !equivalent(eq(node->name, "RE"), eq(pad->name, "RE")))
                    continue;
                const unsigned block = (node->number - 2) / layout.cells, cell = (node->number - 2) % layout.cells;
                if (block >= layout.blocks || cell < 2 + block * layout.cells)
                    continue;
                const unsigned routed = cell - (2 + block * layout.cells);
                unsigned physical     = 0;
                for (unsigned io = 0; io < ios; ++io) {
                    const bool allowed = ios == 16 ? (routed / 8 == io / 8) : ((routed + 16 - 2 * io) % 16 < 8);
                    const auto pin     = layout.io_pins[block * ios + io];
                    if (allowed && !occupied.count(pin)) {
                        physical = pin;
                        break;
                    }
                }
                if (!physical)
                    continue;
                pad->number = physical;
                occupied.insert(physical);
                if (!explicit_pairs.count(node->name)) {
                    node->pair      = pad->name;
                    node->pair_kind = "OPAIR";
                    paired.insert(node->name);
                    paired.insert(pad->name);
                    if (opposite_pair(expression, primary(*pad))) {
                        auto selected = *primary(*pad);
                        for (auto& e : design.equations)
                            if (e.signal == node->name && e.function == selected.function) {
                                const auto phase = e.inverted;
                                selected.signal  = node->name;
                                e                = selected;
                                e.inverted       = phase;
                            }
                    }
                    for (auto& e : design.equations)
                        if (e.signal == node->name)
                            e.minimize = primary(*pad)->minimize;
                }
                break;
            }
    }
    // P_N and PAIR are independent. PAIR=N disables output pairing
    // on all device families, including MACH4xx.
    if (options.automatic_output_pairing)
        for (auto& node : design.pins)
            if (node.node && !paired.count(node.name) && !design.requested_power_up.count(node.name)) {
                auto expression = primary(node);
                if (!expression)
                    continue;
                std::vector<Pin*> matches;
                // A clock, set or reset equation that only one of the two has
                // does not prevent the pairing; the other one receives a copy
                // below. Two different equations do.
                const auto shared_control = [&](const Equation* a, const Equation* b) {
                    return !a || !b || equivalent(a, b);
                };
                // PAIRNEG proves that D-register output phase may differ while
                // the raw physical excitation cover and all controls still match.
                for (auto& p : design.pins)
                    if (!p.node && !paired.count(p.name) && !design.requested_power_up.count(p.name) &&
                        (equivalent(expression, primary(p), expression->function == "REG") ||
                         opposite_pair(expression, primary(p))) &&
                        shared_control(eq(node.name, "C"), eq(p.name, "C")) &&
                        shared_control(eq(node.name, "PR"), eq(p.name, "PR")) &&
                        shared_control(eq(node.name, "RE"), eq(p.name, "RE")))
                        matches.push_back(&p);
                // The candidates are tried in the order in which their own equations
                // appear, not in the order of the pin declarations. The first physically
                // eligible one is taken; an ineligible fixed location does not consume it.
                const auto equation_position = [&](const Pin* pad) {
                    return primary(*pad) - design.equations.data();
                };
                std::stable_sort(matches.begin(), matches.end(), [&](const Pin* a, const Pin* b) {
                    return equation_position(a) < equation_position(b);
                });
                auto selected_pad = std::find_if(matches.begin(), matches.end(),
                                                 [&](Pin* pad) { return normalize_fixed_output_pair(node, *pad); });
                if (selected_pad != matches.end()) {
                    auto& p        = **selected_pad;
                    node.pair      = p.name;
                    node.pair_kind = "OPAIR";
                    paired.insert(node.name);
                    paired.insert(p.name);
                    // Compatibility pairing may select a cover later discarded by the
                    // one-equation policy. Reusing the pin cover for both labels can then
                    // change the node function; normal mode avoids this substitution.
                    if (opposite_pair(expression, primary(p))) {
                        auto selected = *primary(p);
                        for (auto& e : design.equations)
                            if (e.signal == node.name && e.function == selected.function) {
                                auto phase      = e.inverted;
                                selected.signal = node.name;
                                e               = selected;
                                e.inverted      = phase;
                            }
                    }
                    // "Generating ... equation for OUTPUT PAIRING": give each side
                    // the control equations only the other one has.
                    std::vector<Equation> generated;
                    for (const auto* function : {"C", "PR", "RE"}) {
                        const auto* of_node = eq(node.name, function);
                        const auto* of_pad  = eq(p.name, function);
                        if (bool(of_node) == bool(of_pad))
                            continue;
                        generated.push_back(of_node ? *of_node : *of_pad);
                        generated.back().signal = of_node ? p.name : node.name;
                    }
                    for (auto& equation : generated)
                        design.equations.push_back(std::move(equation));
                    // The physical pin's MINIMIZE policy controls a paired macrocell.
                    for (auto& e : design.equations)
                        if (e.signal == node.name)
                            e.minimize = primary(p)->minimize;
                }
            }
    if (options.automatic_input_pairing && design.device.compare(0, 5, "MACH4") == 0)
        for (auto& p : design.pins)
            if (!p.node && !paired.count(p.name)) {
                std::vector<Pin*> matches;
                for (auto& node : design.pins)
                    if (node.node && !paired.count(node.name) && (node.registered || node.latched)) {
                        auto e             = primary(node);
                        bool async_control = false;
                        for (const auto& control : {"PR", "RE"})
                            if (auto c = eq(node.name, control))
                                if (!sum_of_products(c->inverted ? negate(c->expression) : c->expression).empty())
                                    async_control = true;
                        // Dedicated MACH4xx input registers have no asynchronous set or
                        // reset inputs; such nodes must remain ordinary macrocells.
                        if (!async_control && e && !e->inverted && e->expression->kind == ExprKind::Identifier &&
                            e->expression->name == p.name)
                            matches.push_back(&node);
                    }
                // Input pairing follows primary equation insertion order, unlike
                // output pairing's declaration traversal. AMB0..7 distinguish these
                // orders; APC0..3 prove fixed-location filtering precedes selection.
                std::stable_sort(matches.begin(), matches.end(),
                                 [&](const Pin* a, const Pin* b) { return primary(*a) < primary(*b); });
                for (auto* candidate : matches) {
                    auto& node = *candidate;
                    if (node.number) {
                        const auto& layout = mach_device_layout(design.device);
                        const auto base    = layout.blocks * layout.cells + 2;
                        if (node.number < base || node.number - base >= layout.io_pins.size())
                            continue;
                        const auto physical = layout.io_pins[node.number - base];
                        if (p.number && p.number != physical)
                            continue;
                        p.number = physical;
                    } else if (p.number)
                        continue;
                    p.pair      = node.name;
                    p.pair_kind = "IPAIR";
                    paired.insert(p.name);
                    paired.insert(node.name);
                    break;
                }
            }
}

Design parse_design(const std::string& source, FrontendOptions options, const std::string* auxiliary_simulation)
{
    if (source.size() > 4 * 1024 * 1024)
        fail("Source exceeds 4 MiB");
    auto clean_source = [](const std::string& text) {
        std::string clean;
        bool comment = false, quoted = false, metadata = false;
        std::size_t line_start = 0;
        // Header text such as "TITLE Bob's design" is free text up to a
        // comment: an apostrophe there does not open a STRING literal.
        const auto metadata_line = [&](std::size_t at) {
            while (at < text.size() && (text[at] == ' ' || text[at] == '\t'))
                ++at;
            for (const char* keyword : {"TITLE", "PATTERN", "REVISION", "AUTHOR", "COMPANY", "DATE"}) {
                const auto length = std::char_traits<char>::length(keyword);
                bool matches      = at + length <= text.size();
                for (std::size_t i = 0; matches && i < length; ++i)
                    matches = std::toupper(static_cast<unsigned char>(text[at + i])) == keyword[i];
                // Only look past the keyword when it is there: the text may end
                // before a keyword of this length.
                if (matches &&
                    (at + length == text.size() ||
                     !(std::isalnum(static_cast<unsigned char>(text[at + length])) || text[at + length] == '_')))
                    return true;
            }
            return false;
        };
        metadata = metadata_line(0);
        for (std::size_t position = 0; position < text.size(); ++position) {
            const unsigned char c = static_cast<unsigned char>(text[position]);
            if (c == 26)
                break;
            if (c == '\r')
                continue;
            if (c == '\n') {
                comment    = false;
                line_start = position + 1;
                metadata   = !quoted && metadata_line(line_start);
                clean += '\n';
                continue;
            }
            if (comment)
                continue;
            if (c == '\'' && !metadata)
                quoted = !quoted;
            if (c == ';' && !quoted) {
                comment = true;
                continue;
            }
            clean += c;
        }
        if (quoted)
            fail("Unterminated STRING literal");
        return clean;
    };
    const auto clean       = clean_source(source);
    std::string normalized = upper(clean);
    std::smatch section;
    static const std::regex equations_header(R"((^|\n)[ \t]*(EQUATIONS|EQU|STATE)[ \t]*(\n|$))");
    if (!std::regex_search(normalized, section, equations_header))
        fail("Missing EQUATIONS or STATE section");
    auto equations_start     = static_cast<std::size_t>(section.position() + section.length());
    std::string declarations = clean.substr(0, section.position());
    std::string body         = (section[2] == "STATE" ? "STATE\n" : "") + normalized.substr(equations_start);
    Design design;
    design.dos_compatibility = options.dos_compatibility;
    static const std::regex simulation_header(R"((^|\n)[ \t]*(SIMULATION|SIM)[ \t]*(\n|$))");
    if (std::regex_search(body, section, simulation_header)) {
        design.simulation = body.substr(section.position() + section.length());
        body.resize(section.position());
    }
    // STATE/CONDITIONS may precede the single EQUATIONS segment.
    // Process Boolean equations first, then add state-machine equations.
    static const std::regex boolean_header(R"((^|\n)[ \t]*(EQUATIONS|EQU)[ \t]*(\n|$))");
    if (std::regex_search(body, section, boolean_header)) {
        if (section.position() == 0 || body.compare(0, 6, "STATE\n") != 0)
            fail("Multiple EQUATIONS segments are not allowed");
        const auto state     = body.substr(0, section.position());
        const auto equations = body.substr(section.position() + section.length());
        if (std::regex_search(equations, boolean_header))
            fail("Multiple EQUATIONS segments are not allowed");
        body = equations + "\n" + state;
    }
    std::string auxiliary_commands, auxiliary_declarations;
    if (auxiliary_simulation) {
        if (auxiliary_simulation->size() > 4 * 1024 * 1024)
            fail("Auxiliary simulation exceeds 4 MiB");
        const auto auxiliary = upper(clean_source(*auxiliary_simulation));
        if (!std::regex_search(auxiliary, section, simulation_header))
            fail("Auxiliary simulation file requires a SIMULATION section");
        auxiliary_declarations = auxiliary.substr(0, section.position());
        auxiliary_commands     = auxiliary.substr(section.position() + section.length());
    }
    std::map<std::string, std::string> aliases;
    static const std::regex alias(R"((^|\n)[ \t]*STRING[ \t]+([A-Z0-9_]+)[ \t]+'([^']*)')", std::regex::icase);
    std::string filtered;
    std::size_t previous = 0;
    for (std::sregex_iterator i(declarations.begin(), declarations.end(), alias), end; i != end; ++i) {
        auto m = *i;
        filtered += declarations.substr(previous, m.position() - previous) + "\n";
        validate_declared_name(upper(m[2]), "STRING");
        if (!aliases.emplace(upper(m[2]), upper(m[3])).second)
            fail("Duplicate STRING: " + m[2].str());
        previous = m.position() + m.length();
    }
    filtered += declarations.substr(previous);
    std::set<std::string> declared;
    std::map<std::string, std::vector<std::string>> groups;
    // Declarations can share lines, and GROUP/vector locations can wrap.
    std::stringstream raw_lines(filtered);
    std::string raw, joined;
    static const std::regex declaration_start(
        R"(^[ \t]*(TITLE|PATTERN|REVISION|AUTHOR|COMPANY|DATE|CHIP|PIN|NODE|GROUP|SIGNATURE)\b)", std::regex::icase);
    static const std::regex metadata_line(R"(^[ \t]*(TITLE|PATTERN|REVISION|AUTHOR|COMPANY|DATE)\b)", std::regex::icase);
    while (std::getline(raw_lines, raw)) {
        if (trim(raw).empty())
            continue;
        if (std::regex_search(raw, metadata_line)) {
            joined += "\n" + raw;
            continue;
        }
        raw = std::regex_replace(raw, inline_declaration_pattern(), "\n$1 ");
        std::stringstream pieces(raw);
        std::string piece;
        while (std::getline(pieces, piece))
            if (!trim(piece).empty())
                joined += (std::regex_search(piece, declaration_start) ? "\n" : " ") + piece;
    }
    std::stringstream lines(joined);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim(line);
        if (line.empty())
            continue;
        line = std::regex_replace(line, glued_location_pattern(), "$1 ?");
        std::stringstream fields(line);
        std::string key, value;
        fields >> key;
        key = upper(key);
        std::getline(fields, value);
        value = trim(value);
        if (key == "TITLE" || key == "PATTERN" || key == "REVISION" || key == "AUTHOR" || key == "COMPANY" ||
            key == "DATE") {
            if (!design.metadata.emplace(key, value).second)
                fail("Duplicate metadata: " + key);
        } else if (key == "CHIP") {
            std::stringstream chip(upper(value));
            std::string extra;
            if (!design.chip.empty() || !(chip >> design.chip >> design.device) || (chip >> extra))
                fail("Expected CHIP <name> <device>");
            validate_declared_name(design.chip, "CHIP");
            // Everything after this line may rely on a known device.
            if (!frontend_devices().count(design.device))
                fail("Unsupported device: " + design.device);
        } else if (key == "PIN" || key == "NODE") {
            auto declaration = upper(value);
            declaration      = std::regex_replace(declaration, space_after_comma_pattern(), ",");
            declaration      = std::regex_replace(declaration, space_after_bracket_pattern(), "[");
            declaration      = std::regex_replace(declaration, space_before_bracket_pattern(), "]");
            std::stringstream f(declaration);
            std::string location, name, mode;
            f >> location >> name;
            validate_declared_name(name, key);
            bool inv = !name.empty() && name[0] == '/';
            if (inv)
                name.erase(0, 1);
            auto expanded = expand_signal_names(name);
            std::vector<std::string> numbers;
            if (location != "?") {
                // MACHPAR checks every location number while parsing: P90 for
                // a numeral it cannot read, P127/P128 for a location the
                // device does not have. Node 1 is its global node.
                const bool reserved = name == "NC" || name == "GND" || name == "VCC" || name == "VCC1" ||
                                      name == "VCC2" || name == "VCC3";
                const auto device = frontend_devices().find(design.device);
                for (std::size_t at = 0; at < location.size();) {
                    if (!std::isdigit(static_cast<unsigned char>(location[at]))) {
                        ++at;
                        continue;
                    }
                    auto end = at;
                    while (end < location.size() && std::isdigit(static_cast<unsigned char>(location[end])))
                        ++end;
                    // Twenty digits are unreadable to the original; eleven are
                    // merely out of range. Avoid converting what cannot fit.
                    if (end - at > 19)
                        fail("P90: Unexpected number symbol in malformed statement: " + location);
                    const auto number = end - at > 9 ? ~0UL : std::stoul(location.substr(at, end - at));
                    at                = end;
                    if (reserved || device == frontend_devices().end())
                        continue;  // Checked with the reserved name or the device below.
                    if (key == "PIN" && (!number || number > device->second.first))
                        fail("P127: invalid pin number for this device or macro: " + location);
                    if (key == "NODE" && (!number || number > mach_node_limit(mach_device_layout(design.device))))
                        fail("P128: invalid node number for this device or macro: " + location);
                }
                numbers = expand_signal_names("P[" + location + "]");
            }
            if (!numbers.empty() && numbers.size() != expanded.size())
                fail("Pin location/vector width mismatch");
            bool reg = false, lat = false, storage_declared = false;
            std::string pair_kind, pair;
            while (f >> mode) {
                if (mode == "REG" || mode == "REGISTERED" || mode == "REGISTER") {
                    reg              = true;
                    storage_declared = true;
                } else if (mode == "LAT" || mode == "LATCHED" || mode == "LATCH") {
                    lat              = true;
                    storage_declared = true;
                } else if (mode == "COMB" || mode == "COMBINATORIAL") {
                    storage_declared = true;
                } else if (mode == "PAIR" || mode == "IPAIR" || mode == "OPAIR") {
                    // MACHPAR accepts IPAIR only on a pin and OPAIR only on a node.
                    if (mode == (key == "PIN" ? "OPAIR" : "IPAIR"))
                        fail("P88: Unexpected reserved word or operator " + mode);
                    pair_kind = mode;
                    if (!(f >> pair))
                        fail("Missing pairing signal");
                } else
                    fail("Unknown declaration mode: " + mode);
            }
            if (name == "NC" || name == "GND" || name == "VCC" || name == "VCC1" || name == "VCC2" || name == "VCC3") {
                const auto device = frontend_devices().find(design.device);
                if (device == frontend_devices().end())
                    fail("Unsupported device: " + design.device);
                const auto& layout = mach_device_layout(design.device);
                const auto maximum = key == "PIN" ? device->second.first
                                                  : layout.blocks * layout.cells + 1 +
                                                        (design.device[4] == '4' ? layout.io_pins.size() : 0);
                for (const auto& item : numbers) {
                    const auto number = std::stoul(item.substr(2));
                    if (!number || number > maximum)
                        fail("Invalid " + key + " position for " + name);
                    if (name != "NC") {
                        const auto power = name == "GND" ? "GND" : "VCC";
                        std::istringstream list(device->second.second);
                        std::string entry;
                        bool matching = false;
                        while (list >> entry)
                            matching |= entry == std::string(power) + ":" + std::to_string(number);
                        if (!matching)
                            fail("Invalid PIN position for " + name);
                    }
                }
                // Reserved power/NC declarations annotate locations rather than
                // logic signals. Repeats are allowed, and these declarations do not
                // appear in the emitted signal or pairing tables.
                continue;
            }
            auto pairs = pair.empty() ? std::vector<std::string>{} : expand_signal_names(pair);
            if (!pairs.empty() && pairs.size() != expanded.size())
                fail("Pair vector width mismatch");
            for (std::size_t i = 0; i < expanded.size(); ++i) {
                if (!declared.insert(expanded[i]).second)
                    fail("Duplicate signal: " + expanded[i]);
                Pin p;
                p.name             = expanded[i];
                p.number           = numbers.empty() ? 0U : static_cast<unsigned>(std::stoul(numbers[i].substr(2)));
                p.registered       = reg;
                p.node             = key == "NODE";
                p.latched          = lat;
                p.inverted         = inv;
                p.pair_kind        = pair_kind;
                p.storage_declared = storage_declared;
                if (!pairs.empty())
                    p.pair = pairs[i];
                design.pins.push_back(p);
            }
        } else if (key == "GROUP") {
            std::stringstream f(upper(value));
            std::string name, member;
            f >> name;
            std::vector<std::string> members;
            validate_declared_name(name, "GROUP");
            while (f >> member) {
                bool inv = member[0] == '/';
                if (inv)
                    member.erase(0, 1);
                for (auto s : expand_signal_names(member))
                    members.push_back(inv ? "/" + s : s);
            }
            if (members.empty() || !groups.emplace(name, members).second)
                fail("Invalid/duplicate GROUP: " + name);
            design.groups.emplace_back(name, members);
        } else if (key == "SIGNATURE") {
            design.signature = value;
            if (!design.signature.empty() && design.signature[0] == '=')
                design.signature = trim(design.signature.substr(1));
        } else
            fail("Unsupported declaration: " + key);
    }
    if (design.chip.empty())
        fail("Missing CHIP declaration");
    if (design.pins.empty())
        fail("No PIN/NODE declarations");
    design.simulation = expand_aliases(design.simulation, aliases);
    if (auxiliary_simulation) {
        // HELPER inserts the generated .TBL STRING aliases before the
        // auxiliary SIM section. They affect that command stream only.
        auto auxiliary_aliases = aliases;
        std::string remainder;
        std::size_t consumed = 0;
        for (std::sregex_iterator i(auxiliary_declarations.begin(), auxiliary_declarations.end(), alias), end; i != end;
             ++i) {
            auto m = *i;
            remainder += auxiliary_declarations.substr(consumed, m.position() - consumed);
            validate_declared_name(upper(m[2]), "STRING");
            if (!auxiliary_aliases.emplace(upper(m[2]), upper(m[3])).second)
                fail("Duplicate auxiliary STRING: " + m[2].str());
            consumed = m.position() + m.length();
        }
        remainder += auxiliary_declarations.substr(consumed);
        if (!trim(remainder).empty())
            fail("Unsupported declaration before auxiliary SIMULATION section");
        design.simulation += "\n" + expand_aliases(auxiliary_commands, auxiliary_aliases);
    }
    SynthesisParser(design, options, groups).run(expand_aliases(body, aliases));
    resolve_pairing(design, options);
    std::set<std::string> declared_names;
    for (const auto& p : design.pins)
        declared_names.insert(p.name);
    for (const auto& group : design.groups)
        for (auto member : group.second) {
            if (!member.empty() && member[0] == '/')
                member.erase(0, 1);
            if (!declared_names.count(member))
                fail("Undeclared_names GROUP member: " + member);
        }
    return design;
}

std::map<std::string, std::string> pla_signal_aliases(const Design& design)
{
    // The parser abbreviates vector basenames longer than nine characters.
    // Scalar names are not subject to this rule.
    std::map<std::string, std::string> base_aliases, symbol_aliases;
    std::set<std::string> emitted_names;
    unsigned next_alias = 1;
    for (const auto& p : design.pins) {
        auto bracket = p.name.find('[');
        auto name    = p.name;
        if (bracket != std::string::npos && bracket > 9) {
            auto base  = p.name.substr(0, bracket);
            auto found = base_aliases.find(base);
            if (found == base_aliases.end()) {
                if (next_alias > 999)
                    fail("Vector name abbreviation table exhausted");
                std::ostringstream short_name;
                short_name << base.substr(0, 6) << std::setw(3) << std::setfill('0') << next_alias++;
                found = base_aliases.emplace(base, short_name.str()).first;
            }
            name = found->second + p.name.substr(bracket);
        }
        if (!emitted_names.insert(name).second)
            fail("Abbreviated signal name collision: " + name);
        symbol_aliases.emplace(p.name, name);
    }
    return symbol_aliases;
}

std::string emit_pla(const Design& input_design, const std::string& source_name, OptimizerOptions options,
                     Design* expanded_design)
{
    Design design             = input_design;
    options.dos_compatibility = design.dos_compatibility;
    for (const auto& initial : design.requested_power_up) {
        auto found = std::find_if(design.equations.begin(), design.equations.end(), [&](const Equation& e) {
            return e.signal == initial.first && (e.function == "REG" || e.function == "T");
        });
        if (found == design.equations.end())
            fail("START_UP.OUTF requires register initialization: " + initial.first);
        auto& equation = *found;
        if (equation.inverted != initial.second) {
            // Complementing stored Q changes D's required cover, while
            // toggle excitation is unchanged by the physical output phase.
            if (equation.function == "REG")
                equation.expression = negate(equation.expression);
            equation.inverted = initial.second;
            equation.source_clauses.clear();
            equation.opposite_expression.reset();
            equation.opposite_care.reset();
        }
    }
    const bool hardware_xor = design.device.compare(0, 5, "MACH3") == 0 || design.device.compare(0, 5, "MACH4") == 0;
    // Paired source equations share one optimization/gate-split pass. Decide
    // that identity before conversion adds endpoint-specific feedback names.
    std::map<std::string, std::string> paired_gate_owner;
    std::map<std::string, bool> paired_gate_phase;
    if (options.gate_split)
        for (const auto& node : design.pins)
            if (node.node && !node.pair.empty() && node.pair_kind != "IPAIR") {
                auto pad = std::find_if(design.pins.begin(), design.pins.end(),
                                        [&](const Pin& p) { return !p.node && p.name == node.pair; });
                if (pad == design.pins.end())
                    continue;
                auto original = design.output_pair_phases.find(node.name);
                if (original != design.output_pair_phases.end()) {
                    paired_gate_owner[node.name] = pad->name;
                    paired_gate_phase[node.name] = original->second;
                    continue;
                }
                auto primary = [](const Equation& e) {
                    return e.function == "COM" || e.function == "REG" || e.function == "T" || e.function == "G";
                };
                const Equation* ne = nullptr;
                const Equation* pe = nullptr;
                bool same = true, primary_complement = false;
                for (const auto& e : design.equations)
                    if (e.signal == node.name) {
                        auto p = std::find_if(design.equations.begin(), design.equations.end(), [&](const Equation& x) {
                            return x.signal == pad->name && x.function == e.function;
                        });
                        if (p == design.equations.end()) {
                            same = false;
                            break;
                        }
                        auto a = sum_of_products(e.expression), b = sum_of_products(p->expression);
                        std::sort(a.begin(), a.end());
                        std::sort(b.begin(), b.end());
                        if (e.minimize != p->minimize) {
                            same = false;
                            break;
                        }
                        if (a != b) {
                            // Forced phase can complement only one endpoint of a valid
                            // source pair. Preserve its relative logical phase when the
                            // pad's shared gate expansion is copied back to the node.
                            if (!primary(e) || e.function == "T") {
                                same = false;
                                break;
                            }
                            b = sum_of_products(negate(p->expression));
                            std::sort(b.begin(), b.end());
                            if (a != b) {
                                same = false;
                                break;
                            }
                            primary_complement = true;
                        }
                        a = sum_of_products(e.care ? e.care : make_constant(true));
                        b = sum_of_products(p->care ? p->care : make_constant(true));
                        std::sort(a.begin(), a.end());
                        std::sort(b.begin(), b.end());
                        if (a != b) {
                            same = false;
                            break;
                        }
                        if (primary(e)) {
                            ne = &e;
                            pe = &*p;
                        } else if (e.inverted != p->inverted) {
                            same = false;
                            break;
                        }
                    }
                if (same && ne && pe) {
                    paired_gate_owner[node.name] = pad->name;
                    paired_gate_phase[node.name] = (ne->inverted != pe->inverted) != primary_complement;
                }
            }
    std::map<std::string, PreparedXor> prepared_registers;
    std::set<std::string> optimized_inversions;
    std::set<std::string> best_register_signals;
    std::map<std::string, std::size_t> candidate_minimum;
    if (options.best_registers || options.best_jk_rs) {
        std::vector<Equation> candidates;
        for (const auto& e : design.equations) {
            if ((!options.best_registers && e.excitation_origin.empty()) ||
                (e.function != "REG" && e.function != "T") || !e.minimize ||
                design.requested_power_up.count(e.signal)) {
                candidates.push_back(e);
                continue;
            }
            auto d = e, t = e;
            d.function      = "REG";
            t.function      = "T";
            auto& converted = e.function == "REG" ? t : d;
            auto feedback   = make_identifier(e.signal);
            if (e.inverted)
                feedback = negate(feedback);
            auto convert = [&](const ExprPtr& expression) {
                return hardware_xor ? make_binary(ExprKind::Xor, feedback, expression)
                                    : make_binary(ExprKind::Xor, expression, feedback);
            };
            converted.expression = convert(e.expression);
            if (e.opposite_expression)
                converted.opposite_expression = convert(e.opposite_expression);
            converted.source_clauses.clear();
            converted.converted_excitation = true;
            if (!hardware_xor && options.forced_polarity >= 0 && d.inverted != (options.forced_polarity != 0)) {
                d.source_clauses.clear();
                if (d.opposite_expression) {
                    std::swap(d.expression, d.opposite_expression);
                    std::swap(d.care, d.opposite_care);
                    std::swap(d.minimize, d.opposite_minimize);
                } else
                    d.expression = negate(d.expression);
                d.inverted = options.forced_polarity != 0;
            }
            auto raw_count = [](const ExprPtr& expression) {
                return machmin_products(expression, expression_variables(expression)).size();
            };
            if (hardware_xor) {
                auto prepared_d = prepare_xor(d.expression, d.care, d.opposite_expression, !d.converted_excitation);
                auto prepared_t = prepare_xor(t.expression, t.care, t.opposite_expression, !t.converted_excitation);
                candidate_minimum[e.signal] = std::min(prepared_d.minimum_terms(), prepared_t.minimum_terms());
                prepared_registers[e.signal + ".REG"] = std::move(prepared_d);
                prepared_registers[e.signal + ".T"]   = std::move(prepared_t);
            } else
                candidate_minimum[e.signal] = std::min(raw_count(d.expression), raw_count(t.expression));
            best_register_signals.insert(e.signal);
            candidates.push_back(d);
            candidates.push_back(t);
        }
        design.equations = std::move(candidates);
    }

    const auto& devices = frontend_devices();
    auto dev            = devices.find(design.device);
    if (dev == devices.end())
        fail("Unsupported device: " + design.device);
    std::map<unsigned, std::string> supply;
    std::stringstream supplies(dev->second.second);
    std::string label;
    while (supplies >> label) {
        auto c                                  = label.find(':');
        supply[unsigned(std::stoul(label.substr(c + 1)))] = label.substr(0, c);
    }
    std::vector<Pin> pins = design.pins;
    std::stable_sort(pins.begin(), pins.end(), [](const Pin& a, const Pin& b) {
        const auto category = [](const Pin& p) { return (p.number ? 0 : 2) + (p.node ? 1 : 0); };
        if (category(a) != category(b))
            return category(a) < category(b);
        return a.number && b.number && a.number < b.number;
    });
    auto symbol_aliases = pla_signal_aliases(design);
    auto emitted        = [&](const std::string& name) {
        auto found = symbol_aliases.find(name);
        return found == symbol_aliases.end() ? name : found->second;
    };
    std::set<unsigned> placed;
    for (const auto& pin : pins)
        if (pin.number && !pin.node) {
            if (pin.number > dev->second.first || supply.count(pin.number))
                fail("Invalid signal pin for " + design.device + ": " + std::to_string(pin.number));
            if (!placed.insert(pin.number).second)
                fail("Duplicate physical pin assignment");
        }
    std::vector<Equation> equations;
    const std::vector<std::string> functions = {"REG",    "T",      "G",      "COM",    "XORL_REG", "XORR_REG",
                                                "XORL_T", "XORR_T", "XORL_G", "XORR_G", "XORL",     "XORR",
                                                "C",      "PR",     "RE",     "OE"};

    std::vector<Sop> terms;
    std::set<std::string> used;
    std::map<std::string, Sop> minimized;
    std::map<std::string, std::vector<std::string>> ordered_variables;
    std::vector<std::string> signal_order;
    // Visit nodes before pins. Within each kind, visit floating declarations
    // in reverse order, then fixed physical locations in descending order.
    for (bool node : {true, false}) {
        for (auto p = design.pins.rbegin(); p != design.pins.rend(); ++p)
            if (p->node == node && !p->number)
                signal_order.push_back(p->name);
        for (auto p = pins.rbegin(); p != pins.rend(); ++p)
            if (p->node == node && p->number)
                signal_order.push_back(p->name);
    }
    std::set<std::string> phase_flips;
    std::map<std::string, std::vector<Equation>> hardware_replacements;
    OptimizerState state;
    auto expression_from_cover = [](const Sop& cover) {
        auto expression = make_constant(false);
        for (auto cube = cover.rbegin(); cube != cover.rend(); ++cube) {
            auto product = make_constant(true);
            for (const auto& literal : *cube) {
                auto term = make_identifier(literal.first);
                if (!literal.second)
                    term = negate(term);
                product = conjunction(product, term);
            }
            expression = disjunction(product, expression);
        }
        return expression;
    };
    auto finish_signal = [&](const std::string& current_signal) {
        if (!hardware_replacements.empty()) {
            std::vector<Equation> lowered;
            for (const auto& e : design.equations) {
                auto replacement = hardware_replacements.find(e.signal + "." + e.function);
                if (replacement == hardware_replacements.end())
                    lowered.push_back(e);
                else
                    lowered.insert(lowered.end(), replacement->second.begin(), replacement->second.end());
            }
            design.equations = std::move(lowered);
        }
        if (best_register_signals.count(current_signal)) {
            const auto& signal = current_signal;
            auto cost          = [&](const std::string& mode) {
                auto simple = minimized.find(signal + "." + mode);
                if (simple != minimized.end())
                    return simple->second.size();
                auto left = minimized.find(signal + ".XORL_" + mode), right = minimized.find(signal + ".XORR_" + mode);
                if (left != minimized.end() && right != minimized.end())
                    return left->second.size() + right->second.size();
                return std::numeric_limits<std::size_t>::max();
            };
            auto d = cost("REG"), t = cost("T");
            // Discard candidates with an unwanted external phase when a matching
            // candidate exists, then compare term cost. D can adopt the requested
            // phase; T retains its source phase.
            if (options.forced_polarity >= 0 && d != std::numeric_limits<std::size_t>::max()) {
                auto candidate = std::find_if(design.equations.begin(), design.equations.end(), [&](const Equation& e) {
                    return e.signal == signal && (e.function == "T" || e.function == "XORR_T");
                });
                if (candidate != design.equations.end() && candidate->inverted != (options.forced_polarity != 0))
                    t = std::numeric_limits<std::size_t>::max();
            }
            bool prefer_t =
                d == std::numeric_limits<std::size_t>::max() ||
                (t != std::numeric_limits<std::size_t>::max() && prefer_t_candidate(d, t, hardware_xor ? 1 : 4));
            const std::string losing = prefer_t ? "REG" : "T";
            design.equations.erase(std::remove_if(design.equations.begin(), design.equations.end(),
                                                  [&](const Equation& e) {
                                                      return e.signal == signal &&
                                                             (e.function == losing || e.function == "XORL_" + losing ||
                                                              e.function == "XORR_" + losing);
                                                  }),
                                   design.equations.end());
        }
        for (auto& e : design.equations)
            if (e.signal == current_signal && phase_flips.count(e.signal + "." + e.function))
                e.inverted = !e.inverted;
        if (options.forced_polarity >= 0)
            for (auto& e : design.equations)
                if (e.signal == current_signal) {
                    if (e.function != "COM" && e.function != "REG" && e.function != "G" && e.function != "XORL" &&
                        e.function != "XORR" && e.function != "XORL_REG" && e.function != "XORR_REG" &&
                        e.function != "XORL_G" && e.function != "XORR_G")
                        continue;
                    const bool negative = options.forced_polarity != 0;
                    if (e.inverted == negative)
                        continue;
                    const auto key = e.signal + "." + e.function;
                    if (hardware_xor) {
                        if (!optimized_inversions.erase(key))
                            optimized_inversions.insert(key);
                    } else {
                        auto& cover = minimized.at(key);
                        cover       = machmin_products(negate(expression_from_cover(cover)), ordered_variables[key]);
                    }
                    e.inverted = negative;
                }
        if (options.gate_split) {
            const bool mach1       = design.device.compare(0, 5, "MACH1") == 0;
            const bool mach2       = design.device.compare(0, 5, "MACH2") == 0;
            const unsigned maximum = mach1 || design.device == "MACH231" ? 12 : mach2 ? 16 : 20;
            unsigned threshold     = mach1   ? options.gate_threshold_mach1
                                     : mach2 ? options.gate_threshold_mach2
                                             : options.gate_threshold;
            unsigned bucket        = mach1   ? options.gate_bucket_mach1
                                     : mach2 ? options.gate_bucket_mach2
                                             : options.gate_bucket;
            // Limit the bucket to the threshold before applying independent
            // device bounds to the two parameters.
            bucket    = std::min(bucket, threshold);
            bucket    = std::clamp(bucket, mach1 || mach2 ? 4u : 8u, maximum);
            threshold = std::clamp(threshold, mach1 || mach2 ? 8u : 12u, maximum);
            if (mach1 || mach2)
                threshold = maximum;  // Legacy devices use their full product-term capacity.
            unsigned next_node = 0;
            for (const auto& signal : std::vector<std::string>{current_signal})
                for (const auto& function : functions) {
                    if (function == "C" || function == "PR" || function == "RE" || function == "OE")
                        continue;
                    auto found = std::find_if(design.equations.begin(), design.equations.end(), [&](const Equation& e) {
                        return e.signal == signal && e.function == function;
                    });
                    if (found == design.equations.end())
                        continue;
                    const auto index              = std::size_t(found - design.equations.begin());
                    const bool minimize_generated = found->minimize;
                    auto variables                = ordered_variables[signal + "." + function];
                    auto& cover                   = minimized.at(signal + "." + function);
                    unsigned signal_threshold     = threshold;
                    if (!mach1 && !mach2) {
                        const bool registered = function == "REG" || function == "T" || function == "G" ||
                                                function.find("_REG") != std::string::npos ||
                                                function.find("_T") != std::string::npos ||
                                                function.find("_G") != std::string::npos;
                        bool synchronous = false, asynchronous = false;
                        for (const auto& group : design.groups)
                            if (std::find(group.second.begin(), group.second.end(), signal) != group.second.end()) {
                                synchronous  = group.first == "SYNC_LIST";
                                asynchronous = group.first == "ASYNC_LIST";
                                break;
                            }
                        // Use the first group containing the signal.
                        if (!asynchronous && !synchronous) {
                            auto nonzero = [&](const std::string& control) {
                                auto c = minimized.find(signal + "." + control);
                                return c != minimized.end() && !c->second.empty();
                            };
                            synchronous = nonzero("PR") && nonzero("RE");
                            auto clock  = minimized.find(signal + ".C");
                            if (!synchronous && clock != minimized.end() && clock->second.size() == 1 &&
                                clock->second.front().size() == 1) {
                                auto name          = clock->second.front().begin()->first;
                                auto pin           = std::find_if(design.pins.begin(), design.pins.end(),
                                                                  [&](const Pin& p) { return p.name == name; });
                                const auto& clocks = mach_device_layout(design.device).clock_pins;
                                if (pin != design.pins.end() && pin->number &&
                                    (design.device == "MACH465" || !options.product_term_clocks)) {
                                    auto c      = std::find(clocks.begin(), clocks.end(), pin->number);
                                    synchronous = c != clocks.end() && std::size_t(c - clocks.begin()) >= 2;
                                }
                            }
                        }
                        if (registered && (!synchronous || asynchronous))
                            signal_threshold = std::min(signal_threshold, 18u);
                        if (function.compare(0, 3, "XOR") == 0) {
                            auto left = minimized.find(signal + ".XORL" + function.substr(4));
                            if (left != minimized.end() && !left->second.empty() &&
                                !(left->second.size() == 1 && left->second.front().empty()))
                                --signal_threshold;
                        }
                    }
                    if (cover.size() <= signal_threshold)
                        continue;
                    for (const auto& pin : design.pins)
                        if ((pin.name == signal && !pin.pair.empty()) || pin.pair == signal) {
                            bool shared = paired_gate_owner.count(signal);
                            for (const auto& alias : paired_gate_owner)
                                shared |= alias.second == signal;
                            if (!shared)
                                fail("Gate splitting differing paired equations is not implemented");
                        }
                    std::size_t original_remaining = cover.size();
                    while (cover.size() > (signal_threshold < bucket ? signal_threshold : bucket)) {
                        auto count = std::size_t(bucket);
                        if (signal_threshold < bucket) {
                            if (original_remaining < signal_threshold)
                                original_remaining = cover.size();
                            count = std::min(count, original_remaining);
                        }
                        std::string name;
                        do {
                            if (next_node > 299)
                                fail("Gate splitting generated-node name limit exceeded");
                            name = "_N_O_D_E_" + std::to_string(next_node++);
                        } while (std::any_of(design.pins.begin(), design.pins.end(),
                                             [&](const Pin& p) { return p.name == name; }));
                        Sop moved(cover.begin(), cover.begin() + count);
                        cover.erase(cover.begin(), cover.begin() + count);
                        cover.push_back(Cube{{name, true}});
                        original_remaining = original_remaining >= count ? original_remaining - count : 0;
                        Pin pin;
                        pin.name = name;
                        pin.node = true;
                        design.pins.push_back(pin);
                        pins.push_back(pin);
                        Equation node;
                        node.signal   = name;
                        node.function = "COM";
                        node.care     = make_constant(true);
                        node.minimize = minimize_generated;
                        if (!hardware_xor && options.forced_polarity == 1) {
                            moved         = machmin_products(negate(expression_from_cover(moved)), variables);
                            node.inverted = true;
                        }
                        if (minimize_generated) {
                            auto result = minimize_phase(moved, {}, nullptr, variables, true, options, &state);
                            moved       = std::move(result.cover);
                            if (result.inverted) {
                                if (!hardware_xor && !options.best_polarity)
                                    moved = machmin_products(negate(expression_from_cover(moved)), variables);
                                else if (options.best_polarity && (!hardware_xor || options.external_best_polarity))
                                    node.inverted = true;
                                else
                                    optimized_inversions.insert(name + ".COM");
                            }
                        }
                        if (!minimize_generated)
                            std::reverse(moved.begin(), moved.end());
                        if (options.forced_polarity >= 0 && node.inverted != (options.forced_polarity != 0)) {
                            if (hardware_xor) {
                                if (!optimized_inversions.erase(name + ".COM"))
                                    optimized_inversions.insert(name + ".COM");
                            } else
                                moved = machmin_products(negate(expression_from_cover(moved)), variables);
                            node.inverted = options.forced_polarity != 0;
                        }
                        node.expression = expression_from_cover(moved);
                        design.equations.push_back(node);
                        minimized[name + ".COM"] = std::move(moved);
                        variables.push_back(name);
                    }
                    auto& changed      = design.equations[index];
                    changed.expression = expression_from_cover(cover);
                    changed.care       = make_constant(true);
                    changed.source_clauses.clear();
                    changed.opposite_expression.reset();
                    changed.opposite_care.reset();
                }
        }
        hardware_replacements.clear();
    };
    std::set<std::string> processed_signals;
    for (const auto& ordered_signal : signal_order) {
        auto pair         = paired_gate_owner.find(ordered_signal);
        const auto signal = pair == paired_gate_owner.end() ? ordered_signal : pair->second;
        if (!processed_signals.insert(signal).second)
            continue;
        for (const auto& function : functions)
            for (const auto& equation : design.equations) {
                if (equation.signal != signal || equation.function != function)
                    continue;
                const auto prepared = prepared_registers.find(equation.signal + "." + equation.function);
                if (hardware_xor &&
                    (prepared != prepared_registers.end() ||
                     (!equation.opposite_expression &&
                      (equation.function == "COM" || equation.function == "REG" || equation.function == "T" ||
                       equation.function == "G") &&
                      (equation.expression->kind == ExprKind::Xor || equation.expression->kind == ExprKind::Xnor)))) {
                    std::vector<std::string> variables;
                    append_expression_variables(variables, equation.expression->right);
                    append_expression_variables(variables, equation.expression->left);
                    append_expression_variables(variables, equation.care);
                    OptimizedXor result;
                    if (!equation.minimize) {
                        // MINIMIZE_OFF retains both structural legs, including ties
                        // and a multi-product XORL. It only orients a single product
                        // to the left; dual-complement/phase/cost searches are skipped.
                        auto left = equation.expression->left, right = equation.expression->right;
                        auto left_cover  = machmin_unreduced_products(left),
                             right_cover = machmin_unreduced_products(right);
                        const bool right_constant =
                            right_cover.empty() || (right_cover.size() == 1 && right_cover.front().empty());
                        if ((left_cover.size() > 1 && right_cover.size() < 2) || right_constant) {
                            std::swap(left, right);
                            std::swap(left_cover, right_cover);
                        }
                        variables.clear();
                        append_expression_variables(variables, right);
                        append_expression_variables(variables, left);
                        append_expression_variables(variables, equation.care);
                        result.split    = true;
                        result.inverted = equation.expression->kind == ExprKind::Xnor;
                        result.left     = std::move(left_cover);
                        result.right    = std::move(right_cover);
                        if (result.left.empty() || result.right.empty()) {
                            result.split = false;
                            result.plain = result.left.empty() ? result.right : result.left;
                            std::reverse(result.plain.begin(), result.plain.end());
                        }
                        std::reverse(result.right.begin(), result.right.end());
                        check_optimizer_term_limit(result.left.size());
                        check_optimizer_term_limit(result.right.size());
                    } else if (prepared != prepared_registers.end())
                        result = minimize_prepared_xor(prepared->second, candidate_minimum.at(equation.signal), options,
                                                       &state, true);
                    else if (equation.converted_excitation) {
                        auto converted =
                            prepare_xor(equation.expression, equation.care, equation.opposite_expression, false);
                        result = minimize_prepared_xor(converted, converted.minimum_terms(), options, &state, true);
                    } else
                        result = minimize_xor(equation.expression, equation.care, equation.opposite_expression, options,
                                              &state, true);
                    if (!result.available)
                        continue;
                    if (prepared != prepared_registers.end())
                        variables = prepared->second.variables;
                    auto remember_phase = [&](const std::string& key) {
                        if (equation.function != "T" && options.best_polarity && options.external_best_polarity) {
                            if (result.phase_inverted)
                                phase_flips.insert(key);
                            if (result.inverted != result.phase_inverted)
                                optimized_inversions.insert(key);
                        } else if (result.inverted)
                            optimized_inversions.insert(key);
                    };
                    if (result.split) {
                        auto left = equation, right = equation;
                        auto suffix    = equation.function == "REG" ? "_REG"
                                         : equation.function == "T" ? "_T"
                                         : equation.function == "G" ? "_G"
                                                                    : "";
                        left.function  = "XORL" + std::string(suffix);
                        right.function = "XORR" + std::string(suffix);
                        auto lk = left.signal + "." + left.function, rk = right.signal + "." + right.function;
                        minimized[lk]         = std::move(result.left);
                        minimized[rk]         = std::move(result.right);
                        ordered_variables[lk] = variables;
                        ordered_variables[rk] = variables;
                        if (equation.minimize || !(minimized[lk].size() == 1 && minimized[lk].front().empty()))
                            remember_phase(lk);
                        if (equation.minimize || !(minimized[rk].size() == 1 && minimized[rk].front().empty()))
                            remember_phase(rk);
                        hardware_replacements[equation.signal + "." + equation.function] = {left, right};
                    } else {
                        auto key       = equation.signal + "." + equation.function;
                        minimized[key] = std::move(result.plain);
                        remember_phase(key);
                        ordered_variables[key] = variables;
                    }
                    continue;
                }
                Sop cover;
                if (!equation.minimize) {
                    auto clauses = equation.source_clauses;
                    if (clauses.empty())
                        clauses.push_back(equation.expression);
                    for (const auto& clause : clauses) {
                        auto part = machmin_unreduced_products(clause);
                        std::reverse(part.begin(), part.end());
                        cover.insert(cover.end(), part.begin(), part.end());
                    }
                } else
                    cover = expanded_products(equation.expression);
                if (equation.minimize) {
                    auto variables = expression_variables(equation.expression);
                    append_expression_variables(variables, equation.care);
                    bool primary = equation.function == "REG" || equation.function == "COM" || equation.function == "G";
                    bool best_phase =
                        (primary && options.best_polarity) || (hardware_xor && (primary || equation.function == "T"));
                    if (design.requested_power_up.count(equation.signal) && equation.function == "REG")
                        best_phase = false;
                    if (options.best_polarity && primary)
                        append_expression_variables(variables, equation.opposite_expression);
                    ordered_variables[equation.signal + "." + equation.function] = variables;
                    cover = machmin_products(equation.expression, variables);
                    if (best_register_signals.count(equation.signal) &&
                        (equation.function == "REG" || equation.function == "T") &&
                        prune_optimizer_candidate(cover.size(), candidate_minimum.at(equation.signal)))
                        continue;
                    auto dc     = equation.care ? machmin_products(negate(equation.care), variables) : Sop{};
                    auto off    = options.best_polarity && primary && equation.opposite_expression
                                      ? machmin_products(equation.opposite_expression, variables)
                                      : Sop{};
                    auto result = minimize_phase(
                        cover, dc, options.best_polarity && primary && equation.opposite_expression ? &off : nullptr,
                        variables, best_phase, options, &state);
                    cover = std::move(result.cover);
                    if (result.inverted) {
                        auto key = equation.signal + "." + equation.function;
                        if (options.best_polarity && primary && (!hardware_xor || options.external_best_polarity))
                            phase_flips.insert(key);
                        else
                            optimized_inversions.insert(key);
                    }
                }
                check_optimizer_term_limit(cover.size());
                minimized[equation.signal + "." + equation.function] = std::move(cover);
            }
        finish_signal(signal);
        for (const auto& alias : paired_gate_owner)
            if (alias.second == signal) {
                std::vector<Equation> copied;
                for (const auto& e : design.equations)
                    if (e.signal == signal) {
                        auto copy   = e;
                        copy.signal = alias.first;
                        if (copy.function != "C" && copy.function != "PR" && copy.function != "RE" &&
                            copy.function != "OE")
                            copy.inverted = copy.inverted != paired_gate_phase.at(alias.first);
                        const auto source = signal + "." + e.function, target = alias.first + "." + e.function;
                        minimized[target]         = minimized.at(source);
                        ordered_variables[target] = ordered_variables[source];
                        if (optimized_inversions.count(source))
                            optimized_inversions.insert(target);
                        copied.push_back(std::move(copy));
                    }
                design.equations.erase(std::remove_if(design.equations.begin(), design.equations.end(),
                                                      [&](const Equation& e) { return e.signal == alias.first; }),
                                       design.equations.end());
                design.equations.insert(design.equations.end(), copied.begin(), copied.end());
            }
    }
    for (const auto& pin : pins)
        for (const auto& function : functions)
            for (const auto& e : design.equations)
                if (e.signal == pin.name && e.function == function)
                    equations.push_back(e);
    if (equations.size() != design.equations.size())
        fail("Unlowered equation function in PLA emission");
    for (const auto& equation : equations) {
        terms.push_back(minimized.at(equation.signal + "." + equation.function));
        for (const auto& cube : terms.back())
            for (const auto& literal : cube)
                used.insert(literal.first);
    }
    std::vector<std::string> inputs;
    for (const auto& pin : pins)
        if (used.count(pin.name))
            inputs.push_back(pin.name);
    std::vector<std::pair<std::string, std::string>> products;
    for (std::size_t output = 0; output < terms.size(); ++output)
        for (const auto& cube : terms[output]) {
            std::string pattern;
            for (const auto& input : inputs) {
                auto literal = cube.find(input);
                pattern += literal == cube.end() ? '-' : literal->second ? '1' : '0';
            }
            const bool empty_input_bug = design.dos_compatibility && inputs.empty();
            auto found = std::find_if(products.begin(), products.end(),
                                      [&](const auto& p) { return p.first == pattern && p.second[output] != '1'; });
            if (found == products.end()) {
                products.emplace_back(pattern, std::string(equations.size(), '~'));
                found = products.end() - 1;
            }
            found->second[output] = '1';
            // Original constant-variable conversion writes a don't-care at
            // nonexistent input zero, aliasing the first two output bits. Its
            // row merger refuses a row already containing the current output.
            if (empty_input_bug)
                for (std::size_t bit = 0; bit < std::min<std::size_t>(2, equations.size()); ++bit)
                    found->second[bit] = '1';
        }
    std::ostringstream out;
    out << "#$ TOOL AMD 2.7\n#AMD FILENAME " << source_name << '\n';
    for (const auto& key : {"TITLE", "PATTERN", "REVISION", "AUTHOR", "COMPANY", "DATE"}) {
        auto found = design.metadata.find(key);
        if (found != design.metadata.end())
            out << "#AMD " << key << ' ' << found->second << '\n';
    }
    out << "#AMD CHIP " << design.chip << "\n#AMD DEVICE " << design.device << "\n#AMD JED_FILE "
        << source_name.substr(0, source_name.find_last_of('.')) << ".jed\n#AMD SIGNATURE " << design.signature << "\n";
    out << "#AMD PIN " << supply.size() << ' ' << dev->second.second << " \n";
    for (const auto& p : pins) {
        out << "#AMD " << (p.node ? "NODE" : "PIN") << " 1 " << emitted(p.name) << (p.inverted ? "-" : "") << ':'
            << p.number << (p.pair.empty() ? "" : "/" + emitted(p.pair)) << " \n";
    }
    for (const auto& g : design.groups) {
        auto name = g.first;
        if (name.compare(0, 9, "MACH_SEG_") == 0)
            name.erase(0, 9);
        out << "#AMD GROUP " << g.second.size() + 1 << ' ' << name;
        for (const auto& member : g.second)
            out << ' ' << emitted(member);
        out << " \n";
    }
    out << ".type f\n.i " << inputs.size() << "\n.o " << equations.size() << '\n';
    auto labels = [&](const std::string& directive, const std::vector<std::string>& names) {
        out << directive;
        auto column = directive.size();
        for (const auto& name : names) {
            if (column + name.size() + 1 > 75) {
                out << "\n    ";
                column = 4;
            }
            out << ' ' << name;
            column += name.size() + 1;
        }
        out << '\n';
    };
    std::vector<std::string> input_labels;
    for (const auto& name : inputs)
        input_labels.push_back(emitted(name));
    // Compatibility mode omits the mandatory .ilb directive when there
    // is one output and no inputs.
    if (!(design.dos_compatibility && inputs.empty() && equations.size() == 1))
        labels(".ilb", input_labels);
    std::vector<std::string> outputs;
    for (const auto& equation : equations) {
        outputs.push_back(emitted(equation.signal) + (equation.function == "COM" ? "" : "." + equation.function) +
                          (equation.inverted ? "-" : "") +
                          (optimized_inversions.count(equation.signal + "." + equation.function) ? "%" : ""));
    }
    labels(".ob", outputs);
    out << ".phase " << std::string(equations.size(), '1') << "\n.p " << products.size() << '\n';
    for (const auto& product : products)
        out << product.first << ' ' << product.second << '\n';
    out << ".e\n";
    if (expanded_design)
        *expanded_design = options.gate_split ? design : input_design;
    return out.str();
}
}  // namespace machxl
