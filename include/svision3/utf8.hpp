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

    // Decodes the codepoint starting at pos. Malformed sequences decode to the
    // replacement character so callers never have to special-case bad input.
    static uint32_t decode(std::string_view s, size_t pos) {
        if (pos >= s.size()) {
            return 0;
        }
        auto lead = static_cast<uint8_t>(s[pos]);
        auto extra = size_t{0};
        auto cp = uint32_t{0};
        if (lead < 0x80) {
            return lead;
        } else if ((lead & 0xE0) == 0xC0) {
            extra = 1;
            cp = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            extra = 2;
            cp = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            extra = 3;
            cp = lead & 0x07;
        } else {
            return 0xFFFD;
        }
        if (pos + extra >= s.size()) {
            return 0xFFFD;
        }
        for (auto i = size_t{1}; i <= extra; i++) {
            auto c = static_cast<uint8_t>(s[pos + i]);
            if (!is_trailing(c)) {
                return 0xFFFD;
            }
            cp = (cp << 6) | (c & 0x3F);
        }
        return cp;
    }

    static std::string encode(uint32_t cp) {
        auto out = std::string{};
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        return out;
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
