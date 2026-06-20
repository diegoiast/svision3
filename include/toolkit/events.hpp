// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"
#include <string>

namespace toolkit {

struct MouseEvent {
    enum class Type { Press, Release, Move, Drag, Scroll, Leave };
    Type type = Type::Move;
    Point position;
    uint32_t serial = 0;
    // FIXME: we should have constants for button numbers
    int button = 0;
    int click_count = 1;
    bool shift = false;
    bool ctrl = false;
    bool super = false;
    float scroll_dx = 0;
    float scroll_dy = 0;
};

enum class Key {
    NoKey = 0,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Enter,
    Escape,
    Tab,
    LeftAlt,
    RightAlt,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftSuper,
    RightSuper,
    Plus,
    Minus,
    Equals,
    A,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Number0,
    Number1,
    Number2,
    Number3,
    Number4,
    Number5,
    Number6,
    Number7,
    Number8,
    Number9,
};

struct KeyEvent {
    enum class Type { Press, Release };
    Type type = Type::Press;
    Key key = Key::NoKey;
    std::string text;

    // Combined convenience flags — true if either side is pressed
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false;

    // Per-side modifier tracking (individual platforms may fill these)
    bool lshift = false;
    bool rshift = false;
    bool lctrl = false;
    bool rctrl = false;
    bool lalt = false;
    bool ralt = false;
    bool lsuper = false;
    bool rsuper = false;
};

} // namespace toolkit
