// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace svision3 {

class Utf8Iterator {
  public:
    static bool is_trailing(uint8_t c) { return (c & 0xC0) == 0x80; }

    static size_t next(std::string_view s, size_t pos) {
        if (pos >= s.size()) {
            return s.size();
        }
        pos++;
        while (pos < s.size() && is_trailing(static_cast<uint8_t>(s[pos]))) {
            pos++;
        }
        return pos;
    }

    static size_t prev(std::string_view s, size_t pos) {
        if (pos == 0) {
            return 0;
        }
        pos--;
        while (pos > 0 && is_trailing(static_cast<uint8_t>(s[pos]))) {
            pos--;
        }
        return pos;
    }

    static size_t count(std::string_view s) {
        size_t c = 0;
        size_t pos = 0;
        while (pos < s.size()) {
            pos = next(s, pos);
            c++;
        }
        return c;
    }

    static size_t find_char(std::string_view s, size_t char_index) {
        auto pos = size_t{0};
        for (auto i = 0; i < char_index && pos < s.size(); i++) {
            pos = next(s, pos);
        }
        return pos;
    }
};

// Returns true if the string contains no unescaped HTML special characters.
// Allows <br> (produced by html_escape for newlines) and known entities.
inline bool is_html_escaped(std::string_view s) {
    static constexpr std::string_view allowed_tag = "<br>";
    static constexpr std::string_view entities[] = {"&amp;", "&lt;", "&gt;", "&quot;"};
    for (auto i = 0; i < s.size(); ++i) {
        auto c = s[i];
        if (c == '\n') {
            return false;
        }
        if (c == '<' || c == '>' || c == '"') {
            if (s.substr(i, allowed_tag.size()) == allowed_tag) {
                i += allowed_tag.size() - 1;
            } else {
                return false;
            }
        } else if (c == '&') {
            auto found = false;
            for (auto const &e : entities) {
                if (s.substr(i, e.size()) == e) {
                    i += e.size() - 1;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
    }
    return true;
}

inline std::string html_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\n':
            out += "<br>";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

} // namespace svision3
