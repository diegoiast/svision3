// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

namespace toolkit {

struct Point {
    float x = 0;
    float y = 0;
};

struct Size {
    float width = 0;
    float height = 0;
};

struct Rect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

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

    constexpr Color darken(float amount) const {
        auto clamp = [](float v) { return v < 0 ? 0.0f : v; };
        return {clamp(r - amount), clamp(g - amount), clamp(b - amount), a};
    }
    constexpr Color lighten(float amount) const {
        auto clamp = [](float v) { return v > 1 ? 1.0f : v; };
        return {clamp(r + amount), clamp(g + amount), clamp(b + amount), a};
    }
};

struct Margins {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;
};

enum class Alignment { Fill, Start, Center, End };

enum class CursorShape { Arrow, IBeam, Hand, NotAllowed, ResizeEW };

enum class FontFamily { System, Monospace };

} // namespace toolkit
