// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include <cmath>

namespace toolkit {

void Painter::draw_frame(Rect const &rect, Color bg, Color border, WidgetStyle const &style,
                         bool sunken) {
    if (sunken) {
        // Simple sunken effect
        draw_rect(rect, border.darken(0.2f), style.border_width);
        fill_rect(rect.inset(style.border_width), bg.darken(0.05f));
    } else {
        if (style.corner_radius > 0.0f) {
            fill_rounded_rect(rect, bg, style.corner_radius);
            draw_rounded_rect(rect, border, style.corner_radius, style.border_width);
        } else {
            fill_rect(rect, bg);
            draw_rect(rect, border, style.border_width);
        }
    }
}

void Painter::draw_focus_ring(Rect const &rect, float corner_radius) {
    Color ring = Theme::current().line_input.border_focused;
    ring.a = 0.5f;
    if (corner_radius > 0.0f) {
        draw_rounded_rect(rect.inset(-2), ring, corner_radius + 2, 2.0f);
    } else {
        draw_rect(rect.inset(-2), ring, 2.0f);
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
    if (scale <= 0.0f) return val;
    return std::floor(val * scale + 0.5f) / scale;
}

} // namespace toolkit
