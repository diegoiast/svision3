#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string read_file(fs::path const &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "cpp2po: cannot open " << path << "\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string escape_po(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;      break;
        }
    }
    return out;
}

// Parse a C++ string literal starting at pos (which points at the opening ")
// Returns the unescaped content, advances pos past the closing "
static bool parse_string_literal(std::string_view src, size_t &pos,
                                 std::string &out) {
    if (pos >= src.size() || src[pos] != '"') return false;
    ++pos; // skip opening "
    out.clear();
    while (pos < src.size()) {
        char c = src[pos];
        if (c == '"') { ++pos; return true; }
        if (c == '\\' && pos + 1 < src.size()) {
            ++pos;
            switch (src[pos]) {
            case 'n':  out += '\n'; break;
            case 't':  out += '\t'; break;
            case '\\': out += '\\'; break;
            case '"':  out += '"';  break;
            default:   out += '\\'; out += src[pos]; break;
            }
        } else {
            out += c;
        }
        ++pos;
    }
    return false;
}

static void skip_ws(std::string_view src, size_t &pos) {
    while (pos < src.size() &&
           (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' ||
            src[pos] == '\r'))
        ++pos;
}

struct ExtractedString {
    std::string msgid;
    std::string msgid_plural;
    std::string context;
    std::string file;
    int line;

    bool operator<(ExtractedString const &o) const {
        if (msgid != o.msgid) return msgid < o.msgid;
        if (context != o.context) return context < o.context;
        return msgid_plural < o.msgid_plural;
    }
    bool operator==(ExtractedString const &o) const {
        return msgid == o.msgid && msgid_plural == o.msgid_plural &&
               context == o.context;
    }
};

static int line_at(std::string_view src, size_t pos) {
    int line = 1;
    for (size_t i = 0; i < pos && i < src.size(); ++i)
        if (src[i] == '\n') ++line;
    return line;
}

static void extract(std::string_view src, std::string const &filename,
                    std::set<ExtractedString> &out) {
    size_t pos = 0;
    while (pos < src.size()) {
        // Look for tr( or tr_n(
        auto tr_pos = src.find("tr(", pos);
        auto trn_pos = src.find("tr_n(", pos);

        bool is_plural = false;
        size_t match_pos;
        size_t arg_start;

        if (tr_pos == std::string_view::npos &&
            trn_pos == std::string_view::npos)
            break;

        if (trn_pos != std::string_view::npos &&
            (tr_pos == std::string_view::npos || trn_pos <= tr_pos)) {
            // Make sure it's not a longer identifier ending in tr_n
            if (trn_pos > 0 &&
                (std::isalnum(static_cast<unsigned char>(src[trn_pos - 1])) ||
                 src[trn_pos - 1] == '_')) {
                pos = trn_pos + 4;
                continue;
            }
            is_plural = true;
            match_pos = trn_pos;
            arg_start = trn_pos + 5;
        } else {
            // Make sure it's not a longer identifier ending in tr
            if (tr_pos > 0 &&
                (std::isalnum(static_cast<unsigned char>(src[tr_pos - 1])) ||
                 src[tr_pos - 1] == '_')) {
                pos = tr_pos + 3;
                continue;
            }
            // Also make sure this isn't actually tr_n (tr_pos might point at
            // the "tr" in "tr_n")
            if (tr_pos + 2 < src.size() && src[tr_pos + 2] == '_') {
                pos = tr_pos + 3;
                continue;
            }
            is_plural = false;
            match_pos = tr_pos;
            arg_start = tr_pos + 3;
        }

        int ln = line_at(src, match_pos);
        size_t p = arg_start;
        skip_ws(src, p);

        std::string msgid;
        if (!parse_string_literal(src, p, msgid)) {
            pos = p + 1;
            continue;
        }

        if (is_plural) {
            // tr_n("singular", "plural", n [, "context"])
            skip_ws(src, p);
            if (p < src.size() && src[p] == ',') ++p;
            skip_ws(src, p);
            std::string msgid_plural;
            if (!parse_string_literal(src, p, msgid_plural)) {
                pos = p + 1;
                continue;
            }
            // Skip n argument to see if there's a context
            skip_ws(src, p);
            std::string context;
            if (p < src.size() && src[p] == ',') {
                ++p;
                skip_ws(src, p);
                // Skip the n expression — scan until , or )
                int depth = 0;
                while (p < src.size()) {
                    if (src[p] == '(') ++depth;
                    else if (src[p] == ')') {
                        if (depth == 0) break;
                        --depth;
                    } else if (src[p] == ',' && depth == 0)
                        break;
                    ++p;
                }
                if (p < src.size() && src[p] == ',') {
                    ++p;
                    skip_ws(src, p);
                    parse_string_literal(src, p, context);
                }
            }
            out.insert({msgid, msgid_plural, context, filename, ln});
        } else {
            // tr("msgid" [, "context"])
            skip_ws(src, p);
            std::string context;
            if (p < src.size() && src[p] == ',') {
                ++p;
                skip_ws(src, p);
                parse_string_literal(src, p, context);
            }
            out.insert({msgid, {}, context, filename, ln});
        }

        pos = p;
    }
}

static void usage() {
    std::cerr << "Usage: cpp2po -o <output.pot> <file.cpp> [file2.cpp ...]\n"
              << "\n"
              << "Scans C++ sources for tr() and tr_n() calls and generates\n"
              << "a .pot template file.\n"
              << "\n"
              << "Recognized patterns:\n"
              << "  tr(\"msgid\")\n"
              << "  tr(\"msgid\", \"context\")\n"
              << "  tr_n(\"singular\", \"plural\", n)\n"
              << "  tr_n(\"singular\", \"plural\", n, \"context\")\n";
    std::exit(1);
}

int main(int argc, char *argv[]) {
    std::string output;
    std::vector<std::string> inputs;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            usage();
        } else {
            inputs.push_back(arg);
        }
    }
    if (output.empty() || inputs.empty()) usage();

    std::set<ExtractedString> strings;
    for (auto const &path : inputs) {
        auto content = read_file(path);
        extract(content, path, strings);
    }

    std::ofstream out(output);
    if (!out) {
        std::cerr << "cpp2po: cannot write " << output << "\n";
        return 1;
    }

    out << "# Generated by cpp2po\n";
    out << "msgid \"\"\n";
    out << "msgstr \"\"\n";
    out << "\"Content-Type: text/plain; charset=UTF-8\\n\"\n";
    out << "\n";

    for (auto const &s : strings) {
        out << "#: " << s.file << ":" << s.line << "\n";
        if (!s.context.empty())
            out << "msgctxt \"" << escape_po(s.context) << "\"\n";
        out << "msgid \"" << escape_po(s.msgid) << "\"\n";
        if (!s.msgid_plural.empty()) {
            out << "msgid_plural \"" << escape_po(s.msgid_plural) << "\"\n";
            out << "msgstr[0] \"\"\n";
            out << "msgstr[1] \"\"\n";
        } else {
            out << "msgstr \"\"\n";
        }
        out << "\n";
    }

    std::cout << "cpp2po: extracted " << strings.size() << " string(s) to "
              << output << "\n";
    return 0;
}
