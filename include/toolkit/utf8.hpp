// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <string>
#include <string_view>
#include <cstdint>

namespace toolkit {

class Utf8Iterator {
public:
    static bool is_trailing(uint8_t c) {
        return (c & 0xC0) == 0x80;
    }

    static size_t next(std::string_view s, size_t pos) {
        if (pos >= s.size()) return s.size();
        pos++;
        while (pos < s.size() && is_trailing(static_cast<uint8_t>(s[pos]))) {
            pos++;
        }
        return pos;
    }

    static size_t prev(std::string_view s, size_t pos) {
        if (pos == 0) return 0;
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
        size_t pos = 0;
        for (size_t i = 0; i < char_index && pos < s.size(); i++) {
            pos = next(s, pos);
        }
        return pos;
    }
};

} // namespace toolkit
