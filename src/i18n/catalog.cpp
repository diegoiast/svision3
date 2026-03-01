#include "i18n/catalog.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <memory>

namespace i18n {

// ── Plural-Forms expression parser ──────────────────────────────────────────
//
// Parses the C-like expression from the Plural-Forms header, e.g.:
//   "n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2"
//
// Supports: integer literals, 'n', ternary ?:, ||, &&, ==, !=, <, >, <=, >=,
//           +, -, *, /, %, parentheses.

namespace {

struct PluralExpr {
    enum Tag {
        Lit, Var_n,
        Add, Sub, Mul, Div, Mod,
        Eq, Ne, Lt, Gt, Le, Ge,
        And, Or, Ternary, Not
    };
    Tag tag;
    int value = 0;
    std::vector<PluralExpr> children;

    int eval(int n) const {
        switch (tag) {
        case Lit:   return value;
        case Var_n: return n;
        case Add: return children[0].eval(n) + children[1].eval(n);
        case Sub: return children[0].eval(n) - children[1].eval(n);
        case Mul: return children[0].eval(n) * children[1].eval(n);
        case Div: { int r = children[1].eval(n); return r ? children[0].eval(n) / r : 0; }
        case Mod: { int r = children[1].eval(n); return r ? children[0].eval(n) % r : 0; }
        case Eq: return children[0].eval(n) == children[1].eval(n) ? 1 : 0;
        case Ne: return children[0].eval(n) != children[1].eval(n) ? 1 : 0;
        case Lt: return children[0].eval(n) <  children[1].eval(n) ? 1 : 0;
        case Gt: return children[0].eval(n) >  children[1].eval(n) ? 1 : 0;
        case Le: return children[0].eval(n) <= children[1].eval(n) ? 1 : 0;
        case Ge: return children[0].eval(n) >= children[1].eval(n) ? 1 : 0;
        case And: return (children[0].eval(n) && children[1].eval(n)) ? 1 : 0;
        case Or:  return (children[0].eval(n) || children[1].eval(n)) ? 1 : 0;
        case Not: return children[0].eval(n) ? 0 : 1;
        case Ternary:
            return children[0].eval(n) ? children[1].eval(n)
                                       : children[2].eval(n);
        }
        return 0;
    }
};

class ExprParser {
  public:
    explicit ExprParser(std::string_view s) : src_(s), pos_(0) {}

    PluralExpr parse() {
        auto e = parse_ternary();
        return e;
    }

  private:
    std::string_view src_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t'))
            ++pos_;
    }

    bool match(char c) {
        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    bool match(std::string_view s) {
        skip_ws();
        if (pos_ + s.size() <= src_.size() &&
            src_.substr(pos_, s.size()) == s) {
            // For two-char operators, make sure we don't match a prefix
            pos_ += s.size();
            return true;
        }
        return false;
    }

    char peek() {
        skip_ws();
        return pos_ < src_.size() ? src_[pos_] : '\0';
    }

    PluralExpr parse_ternary() {
        auto cond = parse_or();
        skip_ws();
        if (match('?')) {
            auto yes = parse_ternary();
            match(':');
            auto no = parse_ternary();
            PluralExpr e;
            e.tag = PluralExpr::Ternary;
            e.children = {std::move(cond), std::move(yes), std::move(no)};
            return e;
        }
        return cond;
    }

    PluralExpr parse_or() {
        auto left = parse_and();
        while (match("||")) {
            auto right = parse_and();
            PluralExpr e;
            e.tag = PluralExpr::Or;
            e.children = {std::move(left), std::move(right)};
            left = std::move(e);
        }
        return left;
    }

    PluralExpr parse_and() {
        auto left = parse_equality();
        while (match("&&")) {
            auto right = parse_equality();
            PluralExpr e;
            e.tag = PluralExpr::And;
            e.children = {std::move(left), std::move(right)};
            left = std::move(e);
        }
        return left;
    }

    PluralExpr parse_equality() {
        auto left = parse_comparison();
        for (;;) {
            if (match("==")) {
                auto right = parse_comparison();
                PluralExpr e;
                e.tag = PluralExpr::Eq;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else if (match("!=")) {
                auto right = parse_comparison();
                PluralExpr e;
                e.tag = PluralExpr::Ne;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else break;
        }
        return left;
    }

    PluralExpr parse_comparison() {
        auto left = parse_additive();
        for (;;) {
            skip_ws();
            if (match("<=")) {
                auto right = parse_additive();
                PluralExpr e;
                e.tag = PluralExpr::Le;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else if (match(">=")) {
                auto right = parse_additive();
                PluralExpr e;
                e.tag = PluralExpr::Ge;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else if (match('<')) {
                auto right = parse_additive();
                PluralExpr e;
                e.tag = PluralExpr::Lt;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else if (match('>')) {
                auto right = parse_additive();
                PluralExpr e;
                e.tag = PluralExpr::Gt;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else break;
        }
        return left;
    }

    PluralExpr parse_additive() {
        auto left = parse_multiplicative();
        for (;;) {
            if (match('+')) {
                auto right = parse_multiplicative();
                PluralExpr e;
                e.tag = PluralExpr::Add;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else if (peek() == '-' && pos_ + 1 < src_.size() &&
                       src_[pos_ + 1] != '>') {
                ++pos_;
                auto right = parse_multiplicative();
                PluralExpr e;
                e.tag = PluralExpr::Sub;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else break;
        }
        return left;
    }

    PluralExpr parse_multiplicative() {
        auto left = parse_unary();
        for (;;) {
            if (match('*')) {
                auto right = parse_unary();
                PluralExpr e;
                e.tag = PluralExpr::Mul;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else if (match('/')) {
                auto right = parse_unary();
                PluralExpr e;
                e.tag = PluralExpr::Div;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else if (match('%')) {
                auto right = parse_unary();
                PluralExpr e;
                e.tag = PluralExpr::Mod;
                e.children = {std::move(left), std::move(right)};
                left = std::move(e);
            } else break;
        }
        return left;
    }

    PluralExpr parse_unary() {
        if (match('!')) {
            auto operand = parse_unary();
            PluralExpr e;
            e.tag = PluralExpr::Not;
            e.children = {std::move(operand)};
            return e;
        }
        return parse_primary();
    }

    PluralExpr parse_primary() {
        skip_ws();
        if (match('(')) {
            auto e = parse_ternary();
            match(')');
            return e;
        }
        if (pos_ < src_.size() && src_[pos_] == 'n') {
            ++pos_;
            return PluralExpr{PluralExpr::Var_n, 0, {}};
        }
        int val = 0;
        auto start = src_.data() + pos_;
        auto end = src_.data() + src_.size();
        auto [ptr, ec] = std::from_chars(start, end, val);
        if (ec == std::errc{}) {
            pos_ += static_cast<size_t>(ptr - start);
            return PluralExpr{PluralExpr::Lit, val, {}};
        }
        return PluralExpr{PluralExpr::Lit, 0, {}};
    }
};

PluralRule parse_plural_rule(std::string_view expr) {
    if (expr.empty())
        return [](int n) { return n == 1 ? 0 : 1; };

    auto tree = std::make_shared<PluralExpr>(ExprParser(expr).parse());
    return [tree](int n) { return tree->eval(n); };
}

} // anonymous namespace

// ── .po string parsing helpers ──────────────────────────────────────────────

static std::string unescape_po_string(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[++i]) {
            case 'n':  result += '\n'; break;
            case 't':  result += '\t'; break;
            case '\\': result += '\\'; break;
            case '"':  result += '"';  break;
            default:   result += '\\'; result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

static std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' ||
                          s.back() == '\r' || s.back() == '\n'))
        s.remove_suffix(1);
    return s;
}

static std::string extract_quoted(std::string_view line) {
    auto first = line.find('"');
    if (first == std::string_view::npos) return {};
    auto last = line.rfind('"');
    if (last == first) return {};
    return unescape_po_string(line.substr(first + 1, last - first - 1));
}

// ── Catalog key construction ────────────────────────────────────────────────

std::string Catalog::make_key(std::string_view context,
                              std::string_view msgid) {
    if (context.empty()) return std::string(msgid);
    std::string k;
    k.reserve(context.size() + 1 + msgid.size());
    k.append(context);
    k.push_back('\x04'); // gettext context separator
    k.append(msgid);
    return k;
}

// ── Header parsing ──────────────────────────────────────────────────────────

bool Catalog::parse_header(std::string_view header) {
    auto header_str = std::string(header);
    std::istringstream ss(header_str);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.starts_with("Language:")) {
            auto val = trim(std::string_view(line).substr(9));
            language_ = std::string(val);
        } else if (line.starts_with("Plural-Forms:")) {
            auto val = std::string_view(line).substr(13);
            plural_forms_ = std::string(trim(val));
            auto np_pos = val.find("nplurals=");
            if (np_pos != std::string_view::npos) {
                auto num_start = val.data() + np_pos + 9;
                int np = 2;
                std::from_chars(num_start, val.data() + val.size(), np);
                nplurals_ = np;
            }
            auto pl_pos = val.find("plural=");
            if (pl_pos != std::string_view::npos) {
                auto expr_start = pl_pos + 7;
                auto expr_end = val.find(';', expr_start);
                std::string_view expr;
                if (expr_end != std::string_view::npos)
                    expr = val.substr(expr_start, expr_end - expr_start);
                else
                    expr = val.substr(expr_start);
                expr = trim(expr);
                plural_rule_ = parse_plural_rule(expr);
            }
        }
    }
    return true;
}

// ── .po file parser ─────────────────────────────────────────────────────────

bool Catalog::load_string(std::string_view content,
                          bool include_untranslated) {
    entries_.clear();
    language_.clear();
    plural_forms_.clear();
    nplurals_ = 2;
    plural_rule_ = [](int n) { return n == 1 ? 0 : 1; };

    enum State { Idle, InEntry };
    State state = Idle;

    std::string current_context;
    std::string current_msgid;
    std::string current_msgid_plural;
    std::vector<std::string> current_msgstr;
    int current_msgstr_index = -1;

    // Which field the next continuation line appends to
    enum Field { None, Ctx, Id, IdPlural, Str };
    Field last_field = None;

    auto commit = [&] {
        if (current_msgid.empty() && !current_msgstr.empty()) {
            parse_header(current_msgstr[0]);
        } else if (!current_msgid.empty() && !current_msgstr.empty()) {
            bool has_translation = false;
            for (auto &s : current_msgstr) {
                if (!s.empty()) { has_translation = true; break; }
            }
            if (has_translation || include_untranslated) {
                auto key = make_key(current_context, current_msgid);
                Entry entry;
                entry.context = current_context;
                entry.msgid = std::move(current_msgid);
                entry.msgid_plural = std::move(current_msgid_plural);
                entry.msgstr = std::move(current_msgstr);
                entries_[std::move(key)] = std::move(entry);
            }
        }
        current_context.clear();
        current_msgid.clear();
        current_msgid_plural.clear();
        current_msgstr.clear();
        current_msgstr_index = -1;
        last_field = None;
        state = Idle;
    };

    auto content_str = std::string(content);
    std::istringstream ss(content_str);
    std::string raw_line;
    while (std::getline(ss, raw_line)) {
        auto line = trim(raw_line);

        if (line.empty()) {
            if (state == InEntry) commit();
            continue;
        }

        if (line.starts_with("#")) continue;

        if (line.starts_with("msgctxt ")) {
            if (state == InEntry) commit();
            state = InEntry;
            current_context = extract_quoted(line);
            last_field = Ctx;
        } else if (line.starts_with("msgid_plural ")) {
            current_msgid_plural = extract_quoted(line);
            last_field = IdPlural;
        } else if (line.starts_with("msgid ")) {
            if (state == InEntry && !current_msgid.empty()) commit();
            state = InEntry;
            current_msgid = extract_quoted(line);
            last_field = Id;
        } else if (line.starts_with("msgstr[")) {
            auto bracket = line.find(']');
            if (bracket != std::string_view::npos) {
                int idx = 0;
                auto num = line.substr(7, bracket - 7);
                std::from_chars(num.data(), num.data() + num.size(), idx);
                auto val = extract_quoted(line);
                while (static_cast<int>(current_msgstr.size()) <= idx)
                    current_msgstr.emplace_back();
                current_msgstr[idx] = val;
                current_msgstr_index = idx;
                last_field = Str;
            }
        } else if (line.starts_with("msgstr ")) {
            auto val = extract_quoted(line);
            current_msgstr = {val};
            current_msgstr_index = 0;
            last_field = Str;
        } else if (line.starts_with("\"") && line.ends_with("\"")) {
            auto continuation = unescape_po_string(
                line.substr(1, line.size() - 2));
            switch (last_field) {
            case Ctx:      current_context += continuation; break;
            case Id:       current_msgid += continuation; break;
            case IdPlural: current_msgid_plural += continuation; break;
            case Str:
                if (current_msgstr_index >= 0 &&
                    current_msgstr_index < static_cast<int>(current_msgstr.size()))
                    current_msgstr[current_msgstr_index] += continuation;
                break;
            case None: break;
            }
        }
    }

    if (state == InEntry) commit();
    return true;
}

bool Catalog::load(std::string const &path, bool include_untranslated) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    return load_string(ss.str(), include_untranslated);
}

// ── Serialization ───────────────────────────────────────────────────────────

static std::string escape_po_string(std::string_view s) {
    std::string result;
    result.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
        case '\\': result += "\\\\"; break;
        case '"':  result += "\\\""; break;
        case '\n': result += "\\n";  break;
        case '\t': result += "\\t";  break;
        default:   result += c;      break;
        }
    }
    return result;
}

std::string Catalog::to_po_string() const {
    std::string out;

    out += "msgid \"\"\n";
    out += "msgstr \"\"\n";
    if (!language_.empty())
        out += "\"Language: " + language_ + "\\n\"\n";
    if (!plural_forms_.empty())
        out += "\"Plural-Forms: " + plural_forms_ + "\\n\"\n";
    out += "\n";

    for (auto const &[key, entry] : entries_) {
        if (!entry.context.empty())
            out += "msgctxt \"" + escape_po_string(entry.context) + "\"\n";
        out += "msgid \"" + escape_po_string(entry.msgid) + "\"\n";
        if (!entry.msgid_plural.empty()) {
            out += "msgid_plural \"" + escape_po_string(entry.msgid_plural) +
                   "\"\n";
            for (size_t i = 0; i < entry.msgstr.size(); ++i)
                out += "msgstr[" + std::to_string(i) + "] \"" +
                       escape_po_string(entry.msgstr[i]) + "\"\n";
        } else if (!entry.msgstr.empty()) {
            out += "msgstr \"" + escape_po_string(entry.msgstr[0]) + "\"\n";
        }
        out += "\n";
    }
    return out;
}

// ── Merge ───────────────────────────────────────────────────────────────────

void Catalog::merge(Catalog const &other) {
    if (!other.language_.empty())
        language_ = other.language_;
    if (other.plural_rule_) {
        nplurals_ = other.nplurals_;
        plural_rule_ = other.plural_rule_;
    }
    for (auto const &[key, entry] : other.entries_)
        entries_[key] = entry;
}

bool Catalog::merge_file(std::string const &path,
                         bool include_untranslated) {
    Catalog tmp;
    if (!tmp.load(path, include_untranslated)) return false;
    merge(tmp);
    return true;
}

bool Catalog::merge_string(std::string_view content,
                           bool include_untranslated) {
    Catalog tmp;
    if (!tmp.load_string(content, include_untranslated)) return false;
    merge(tmp);
    return true;
}

// ── Lookup ──────────────────────────────────────────────────────────────────

std::string_view Catalog::lookup(std::string_view msgid,
                                 std::string_view context) const {
    auto key = make_key(context, msgid);
    auto it = entries_.find(key);
    if (it != entries_.end() && !it->second.msgstr.empty() &&
        !it->second.msgstr[0].empty())
        return it->second.msgstr[0];
    return msgid;
}

std::string_view Catalog::lookup_plural(std::string_view msgid,
                                        std::string_view msgid_plural, int n,
                                        std::string_view context) const {
    auto key = make_key(context, msgid);
    auto it = entries_.find(key);
    if (it != entries_.end() && !it->second.msgstr.empty()) {
        int idx = plural_rule_ ? plural_rule_(n) : (n == 1 ? 0 : 1);
        idx = std::clamp(idx, 0, static_cast<int>(it->second.msgstr.size()) - 1);
        if (!it->second.msgstr[idx].empty())
            return it->second.msgstr[idx];
    }
    return n == 1 ? msgid : msgid_plural;
}

} // namespace i18n
