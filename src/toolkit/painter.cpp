// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/text_rasterizer.hpp"

#include <cmath>

namespace toolkit {

void Painter::draw_filled_frame(Rect const &rect, Color bg, Color border, const Palette &palette,
                                bool sunken, bool bottom_shadow) {
    if (palette.beveled) {
        if (palette.corner_radius > 0.0f) {
            fill_rounded_rect(rect, bg, palette.corner_radius);
        } else {
            fill_rect(rect, bg);
        }

        if (palette.border_width <= 0) {
            return;
        }

        auto top_left = sunken ? palette.shadow : palette.highlight;
        auto bottom_right = sunken ? palette.highlight : palette.shadow;

        draw_line({rect.x, rect.y}, {rect.x + rect.width - 1, rect.y}, top_left, 1.0f);
        draw_line({rect.x, rect.y}, {rect.x, rect.y + rect.height - 1}, top_left, 1.0f);
        draw_line({rect.x + rect.width - 1, rect.y},
                  {rect.x + rect.width - 1, rect.y + rect.height - 1}, bottom_right, 1.0f);
        draw_line({rect.x, rect.y + rect.height - 1},
                  {rect.x + rect.width - 1, rect.y + rect.height - 1}, bottom_right, 1.0f);
    } else {
        auto bw = palette.border_width;

        if (palette.corner_radius > 0.0f) {
            fill_rounded_rect(rect, bg, palette.corner_radius);
        } else {
            fill_rect(rect, bg);
        }

        if (bw > 0) {
            auto inset = bw / 2.0f;
            auto border_rect = rect.inset(inset);

            if (palette.corner_radius > 0.0f) {
                draw_rounded_rect(border_rect, border,
                                  std::max(0.0f, palette.corner_radius - inset), bw);
            } else {
                draw_rect(border_rect, border, bw);
            }
        }

        // Draw the bottom line, with corners, a pixel down, used by Plasma and GNome
        if (bw > 0 && bottom_shadow) {
            auto r = palette.corner_radius;
            push_clip({rect.x, rect.y + rect.height - r, rect.width, r});
            draw_rounded_rect({rect.x, rect.y - 1, rect.width, rect.height}, palette.dark_shadow, r,
                              bw);
            pop_clip();
        }
    }
}

void Painter::draw_focus_ring(Rect const &rect, float corner_radius) {
    auto lw = 2.0f;
    // Draw 1 pixel inside the rectangle to ensure it's fully visible and not clipped
    auto inset = lw / 2.0f + 0.5f;
    auto ring = Theme::current().palette.accent;
    auto r = rect.inset(inset);
    ring.a = 0.5f;
    if (corner_radius > 0.0f) {
        draw_rounded_rect(r, ring, std::max(0.0f, corner_radius - inset), lw);
    } else {
        draw_rect(r, ring, lw);
    }
}

float Painter::snap_to_pixel(float val, float scale) {
    if (scale <= 0.0f) {
        return val;
    }
    return std::floor(val * scale + 0.5f) / scale;
}

Size Painter::measure_text(std::string_view text, float font_size, FontFamily family) {
    if (!rasterizer_ || text.empty()) {
        return {0, 0};
    }
    return rasterizer_->measure(text, font_size, family);
}

Painter::FontMetrics Painter::font_metrics(float font_size, FontFamily family) {
    if (rasterizer_) {
        return rasterizer_->metrics(font_size, family);
    }
    return {};
}


} // namespace toolkit
