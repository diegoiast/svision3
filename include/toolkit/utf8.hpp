// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace toolkit {

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

    static size_t find_char(std::string_view s, size_t char_index) {
        auto pos = size_t{0};
        for (auto i = 0; i < char_index && pos < s.size(); i++) {
            pos = next(s, pos);
        }
        return pos;
    }
};

// Returns true for codepoints in RTL script ranges:
// Hebrew, Arabic, Syriac, Thaana (0x0590-0x08FF),
// Hebrew & Arabic presentation forms (0xFB1D-0xFDFF),
// Arabic presentation forms (0xFE70-0xFEFF),
// Arabic extended-A (0x10E60-0x10E7F).
inline auto is_rtl_codepoint(uint32_t cp) -> bool {
    return (cp >= 0x0590 && cp <= 0x08FF) || (cp >= 0xFB1D && cp <= 0xFDFF) ||
           (cp >= 0xFE70 && cp <= 0xFEFF) || (cp >= 0x10E60 && cp <= 0x10E7F);
}

// Detect paragraph direction from the first strong directional character.
// Digits, spaces, and punctuation are weak/neutral and do not set the direction.
// Returns true when the first strong character is RTL.
// Returns false for LTR-leading or direction-neutral text.
inline auto paragraph_is_rtl(std::string_view s) -> bool {
    auto pos = size_t{0};
    while (pos < s.size()) {
        auto c = static_cast<uint8_t>(s[pos]);
        uint32_t cp{};
        if ((c & 0x80) == 0) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && pos + 1 < s.size()) {
            cp = ((c & 0x1F) << 6) | (static_cast<uint8_t>(s[pos + 1]) & 0x3F);
        } else if ((c & 0xF0) == 0xE0 && pos + 2 < s.size()) {
            cp = ((c & 0x0F) << 12) | ((static_cast<uint8_t>(s[pos + 1]) & 0x3F) << 6) |
                 (static_cast<uint8_t>(s[pos + 2]) & 0x3F);
        } else if ((c & 0xF8) == 0xF0 && pos + 3 < s.size()) {
            cp = ((c & 0x07) << 18) | ((static_cast<uint8_t>(s[pos + 1]) & 0x3F) << 12) |
                 ((static_cast<uint8_t>(s[pos + 2]) & 0x3F) << 6) |
                 (static_cast<uint8_t>(s[pos + 3]) & 0x3F);
        }
        if (is_rtl_codepoint(cp)) {
            return true;
        }
        // Strong LTR: ASCII Latin and Latin Extended
        if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
            (cp >= 0x00C0 && cp <= 0x024F)) {
            return false;
        }
        // Weak/neutral ASCII: digits, spaces, punctuation → skip
        if (cp < 0x80) {
            pos = Utf8Iterator::next(s, pos);
            continue;
        }
        // All other non-ASCII codepoints are treated as strong LTR
        return false;
    }
    return false;
}

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

} // namespace toolkit
