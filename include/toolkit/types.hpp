// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstdint>
#include <string>

namespace toolkit {

struct Point {
    float x = 0;
    float y = 0;
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
    float r = 0;
    float g = 0;
    float b = 0;
    float a = 1.0f;

    static constexpr Color rgb(float r, float g, float b) { return {r, g, b, 1.0f}; }
    static constexpr Color rgba(float r, float g, float b, float a) { return {r, g, b, a}; }
    static constexpr Color from_argb(uint32_t argb) {
        return {
            static_cast<float>((argb >> 16) & 0xff) / 255.0f,
            static_cast<float>((argb >> 8) & 0xff) / 255.0f,
            static_cast<float>(argb & 0xff) / 255.0f,
            static_cast<float>((argb >> 24) & 0xff) / 255.0f,
        };
    }

    constexpr Color darken(float amount) const {
        auto clamp = [](float v) { return v < 0 ? 0.0f : v; };
        return {clamp(r - amount), clamp(g - amount), clamp(b - amount), a};
    }
    constexpr Color lighten(float amount) const {
        auto clamp = [](float v) { return v > 1 ? 1.0f : v; };
        return {clamp(r + amount), clamp(g + amount), clamp(b + amount), a};
    }

    constexpr float luma() const { return 0.299f * r + 0.587f * g + 0.114f * b; }

    // Linear interpulation between 2 colors, with ratio = t;
    static constexpr Color lerp(Color a, Color b, float t) {
        return Color::rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                           a.a + (b.a - a.a) * t);
    };

    static constexpr Color mid(Color a, Color b) { return Color::lerp(a, b, 0.5); }

    static constexpr Color with_gray(float v) { return Color::rgb(v, v, v); }

    constexpr Color with_alpha(float new_alpha) const { return {r, g, b, new_alpha}; }
};

struct Margins {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;
};

enum class Alignment { Fill, Start, Center, End };

enum class CheckState { Unchecked, Checked, Partial };

enum class CursorShape { Arrow, IBeam, Hand, NotAllowed, ResizeEW };

enum class FontFamily { System, Monospace };

struct SystemFonts {
    std::string system;
    std::string monospace;
    float font_size = 0;
    float auto_repeat_delay = 0;
    float auto_repeat_interval = 0;
};

} // namespace toolkit
