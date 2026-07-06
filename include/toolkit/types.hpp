// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace toolkit {

struct Point {
    float x = 0;
    float y = 0;

    constexpr Point operator-(Point const &other) const { return {x - other.x, y - other.y}; }
    constexpr Point operator+(Point const &other) const { return {x + other.x, y + other.y}; }
};

struct Size {
    float width = 0;
    float height = 0;

    bool operator==(const Size &other) const {
        return width == other.width && height == other.height;
    }
};

struct Rect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    bool operator==(const Rect &other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }

    bool contains(Point p) const {
        return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
    }

    constexpr Rect inset(float amount) const {
        return {x + amount, y + amount, width - 2 * amount, height - 2 * amount};
    }
};

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    static constexpr Color rgb(float r, float g, float b) { return {r, g, b, 1.0f}; }
    static constexpr Color rgba(float r, float g, float b, float a) { return {r, g, b, a}; }
    static constexpr Color from_rgb(uint32_t argb) {
        return {
            static_cast<float>((argb >> 16) & 0xff) / 255.0f,
            static_cast<float>((argb >> 8) & 0xff) / 255.0f,
            static_cast<float>(argb & 0xff) / 255.0f,
            1,
        };
    }

    static constexpr Color from_argb(uint32_t argb) {
        return {
            static_cast<float>((argb >> 16) & 0xff) / 255.0f,
            static_cast<float>((argb >> 8) & 0xff) / 255.0f,
            static_cast<float>(argb & 0xff) / 255.0f,
            static_cast<float>((argb >> 24) & 0xff) / 255.0f,
        };
    }
    static constexpr Color mid(Color a, Color b) { return Color::lerp(a, b, 0.5); }
    static constexpr Color with_gray(float v) { return Color::rgb(v, v, v); }

    constexpr Color with_alpha(float new_alpha) const { return {r, g, b, new_alpha}; }

    constexpr Color darken(float amount) const {
        auto clamp = [](float v) { return v < 0 ? 0.0f : v; };
        return {clamp(r - amount), clamp(g - amount), clamp(b - amount), a};
    }

    constexpr Color lighten(float amount) const {
        auto clamp = [](float v) { return v > 1 ? 1.0f : v; };
        return {clamp(r + amount), clamp(g + amount), clamp(b + amount), a};
    }

    // Brightness of an color, Rec. 601
    // https://en.wikipedia.org/wiki/Luma_(video)
    constexpr float luma() const { return 0.299f * r + 0.587f * g + 0.114f * b; }

    // Linear interpulation between 2 colors, with ratio = t;
    static constexpr Color lerp(Color a, Color b, float t) {
        return Color::rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                           a.a + (b.a - a.a) * t);
    };
};

struct Margins {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;
};

enum class Orientation { Horizontal, Vertical };

enum class Alignment { Fill, Start, Center, End };

enum class TabOrientation { North, South, East, West, WestVertical, EastVertical };

enum class CheckState { Unchecked, Checked, Partial };

enum class CursorShape {
    Arrow,
    IBeam,
    Hand,
    NotAllowed,
    ResizeEW,
    ResizeNS,
    ResizeNW,
    ResizeNESW,
    Move
};

enum class FontFamily { System, Monospace };

struct SystemFonts {
    std::string system;
    std::string monospace;
    float size = 0;
    float auto_repeat_delay = 0;
    float auto_repeat_interval = 0;
};

namespace detail {
// Backing storage for Application::set_force_csd()/force_csd(). Declared here rather than in
// application.hpp so WindowOptions's default member initializer for `csd` can read it without
// types.hpp depending on application.hpp (which itself depends on window.hpp -> types.hpp).
inline bool &default_force_csd() {
    static bool value = false;
    return value;
}
} // namespace detail

struct WindowOptions {
    // Defaults to whatever Application::set_force_csd() last set (false until called). Give an
    // explicit `.csd = true` or `.csd = false` at a create_window() call site to override the
    // app-wide default for just that window -- aggregate init skips this default member
    // initializer entirely for any field given an explicit value, so an explicit false really
    // means false even while the app-wide default is true.
    bool csd = detail::default_force_csd();
    bool frameless = false;
    // Enforced both by the CSD title bar (hides the corresponding button, and
    // Window::minimize/maximize/start_system_move/start_system_resize become no-ops) and, where
    // the platform allows it, by the native window manager itself, even when using native
    // decorations. Useful for utility windows (message boxes, file dialogs) that should stay
    // fixed-size and not be confused with regular top-level windows.
    bool resizable = true;
    bool movable = true;
    bool minimizable = true;
    bool maximizable = true;
    bool closable = true;
};

enum class WindowEdge {
    None,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

enum class DecorationButton { Minimize, Maximize, Restore, Close, Menu };

struct MnemonicInfo {
    std::string text;  // display text with & removed
    std::string key;   // UTF-8 mnemonic character, ASCII-lowercased (empty if none)
};

MnemonicInfo parse_mnemonic(std::string_view text);
std::string strip_mnemonic(std::string_view text);
// Extracts the first UTF-8 codepoint from char_text and returns it lowercased
// (ASCII only; non-ASCII codepoints are returned as-is). Used to normalize a
// raw key-event character so it can be compared against a stored mnemonic_key_.
std::string normalize_mnemonic_key(std::string_view char_text);

} // namespace toolkit
