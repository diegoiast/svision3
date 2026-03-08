// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/command.hpp"
#include "toolkit/events.hpp"
#include <algorithm>
#include <cctype>
#include <vector>

namespace toolkit {

static std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = s.find(delim);
    while (end != std::string::npos) {
        result.emplace_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(delim, start);
    }
    result.emplace_back(s.substr(start));
    return result;
}

Shortcut Shortcut::parse(std::string_view s) {
    Shortcut result;
    auto parts = split(s, '+');

    for (auto &p : parts) {
        std::string low;
        low.reserve(p.size());
        for (char c : p) {
            low += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (low == "ctrl" || low == "control") {
            result.ctrl = true;
        } else if (low == "alt") {
            result.alt = true;
        } else if (low == "shift") {
            result.shift = true;
        } else if (low == "super" || low == "meta" || low == "cmd" || low == "command") {
            result.super = true;
        } else if (low == "backspace") {
            result.key = Key::Backspace;
        } else if (low == "delete") {
            result.key = Key::Delete;
        } else if (low == "left") {
            result.key = Key::Left;
        } else if (low == "right") {
            result.key = Key::Right;
        } else if (low == "up") {
            result.key = Key::Up;
        } else if (low == "down") {
            result.key = Key::Down;
        } else if (low == "home") {
            result.key = Key::Home;
        } else if (low == "end") {
            result.key = Key::End;
        } else if (low == "pageup") {
            result.key = Key::PageUp;
        } else if (low == "pagedown") {
            result.key = Key::PageDown;
        } else if (low == "enter" || low == "return") {
            result.key = Key::Enter;
        } else if (low == "escape" || low == "esc") {
            result.key = Key::Escape;
        } else if (low == "tab") {
            result.key = Key::Tab;
        } else if (low == "f1") {
            result.key = Key::F1;
        } else if (low == "f2") {
            result.key = Key::F2;
        } else if (low == "f3") {
            result.key = Key::F3;
        } else if (low == "f4") {
            result.key = Key::F4;
        } else if (low == "f5") {
            result.key = Key::F5;
        } else if (low == "f6") {
            result.key = Key::F6;
        } else if (low == "f7") {
            result.key = Key::F7;
        } else if (low == "f8") {
            result.key = Key::F8;
        } else if (low == "f9") {
            result.key = Key::F9;
        } else if (low == "f10") {
            result.key = Key::F10;
        } else if (low == "f11") {
            result.key = Key::F11;
        } else if (low == "f12") {
            result.key = Key::F12;
        } else if (!low.empty()) {
            result.character = low[0];
        }
    }
    return result;
}

bool Shortcut::matches(KeyEvent const &event) const {
    if (is_empty() || event.type != KeyEvent::Type::Press) {
        return false;
    }

    if (event.ctrl != ctrl || event.alt != alt || event.shift != shift || event.super != super) {
        return false;
    }

    if (key != Key::NoKey) {
        return event.key == key;
    }

    if (character != 0 && !event.text.empty()) {
        char ev_ch = static_cast<char>(std::tolower(static_cast<unsigned char>(event.text[0])));
        return ev_ch == character;
    }

    return false;
}

void Command::set_shortcut(std::string s) {
    shortcut_string_ = std::move(s);
    shortcut_ = Shortcut::parse(shortcut_string_);
}

} // namespace toolkit
