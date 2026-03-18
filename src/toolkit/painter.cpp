// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include <cmath>

namespace toolkit {

void Painter::draw_frame(Rect const &rect, Color bg, Color border, WidgetStyle const &style,
                         bool sunken) {
    // Fill background first at full size
    if (style.corner_radius > 0.0f) {
        fill_rounded_rect(rect, bg, style.corner_radius);
    } else {
        fill_rect(rect, bg);
    }

    if (style.border_width <= 0) {
        return;
    }

    if (style.beveled) {
        // Simple 3D bevel using highlight and shadow from style
        Color top_left = sunken ? style.shadow : style.highlight;
        Color bottom_right = sunken ? style.highlight : style.shadow;

        // Outer lines
        draw_line({rect.x, rect.y}, {rect.x + rect.width - 1, rect.y}, top_left, 1.0f);
        draw_line({rect.x, rect.y}, {rect.x, rect.y + rect.height - 1}, top_left, 1.0f);
        draw_line({rect.x + rect.width - 1, rect.y},
                  {rect.x + rect.width - 1, rect.y + rect.height - 1}, bottom_right, 1.0f);
        draw_line({rect.x, rect.y + rect.height - 1},
                  {rect.x + rect.width - 1, rect.y + rect.height - 1}, bottom_right, 1.0f);
    } else {
        // Draw flat border just inside the rectangle to avoid clipping issues.
        // Stroke is centered on the path, so we inset by half the width.
        auto inset = style.border_width / 2.0f;
        auto border_rect = rect.inset(inset);
        auto border_c = sunken ? border.darken(0.2f) : border;

        if (style.corner_radius > 0.0f) {
            draw_rounded_rect(border_rect, border_c, std::max(0.0f, style.corner_radius - inset),
                              style.border_width);
        } else {
            draw_rect(border_rect, border_c, style.border_width);
        }
    }
}

void Painter::draw_focus_ring(Rect const &rect, float corner_radius) {
    Color ring = Theme::current().line_input.border_focused;
    ring.a = 0.5f;
    float lw = 2.0f;
    // Draw 1 pixel inside the rectangle to ensure it's fully visible and not clipped
    float inset = lw / 2.0f + 0.5f;
    Rect r = rect.inset(inset);
    if (corner_radius > 0.0f) {
        draw_rounded_rect(r, ring, std::max(0.0f, corner_radius - inset), lw);
    } else {
        draw_rect(r, ring, lw);
    }
}

Size Painter::measure_text(std::string_view text, float font_size, FontFamily font) {
    if (auto *p = detail::current_platform()) {
        return p->measure_text(text, font_size, font);
    }
    return {0, 0};
}

Painter::FontMetrics Painter::measure_font_metrics(float font_size, FontFamily font) {
    if (auto *p = detail::current_platform()) {
        return p->measure_font_metrics(font_size, font);
    }
    return {0, 0, 0};
}

float Painter::snap_to_pixel(float val, float scale) {
    if (scale <= 0.0f) {
        return val;
    }
    return std::floor(val * scale + 0.5f) / scale;
}

} // namespace toolkit
