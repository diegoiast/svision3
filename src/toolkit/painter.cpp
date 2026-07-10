// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/text/bidi.hpp"
#include "toolkit/text/text_layout.hpp"
#include "toolkit/text_rasterizer.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"

#include <cmath>

namespace toolkit {

void Painter::draw_filled_frame(Rect const &rect, Color bg, Color border, const Palette &palette,
                                bool sunken, bool bottom_shadow) {
    auto &style = Theme::current().style;
    if (style.beveled) {
        if (style.corner_radius > 0.0f) {
            fill_rounded_rect(rect, bg, style.corner_radius);
        } else {
            fill_rect(rect, bg);
        }

        if (style.border_width <= 0) {
            return;
        }

        // FIXME: should we move this to the theme instead?
        if (style.border_width >= 2.0f) {
            // 2-pixel bevel (Windows 95 style)
            auto c_tl_outer = sunken ? palette.shadow : palette.light;
            auto c_tl_inner = sunken ? palette.dark_shadow : palette.window;
            auto c_br_inner = sunken ? palette.window : palette.shadow;
            auto c_br_outer = sunken ? palette.light : palette.dark_shadow;

            auto x = rect.x;
            auto y = rect.y;
            auto w = rect.width;
            auto h = rect.height;

            // Outer bevel
            draw_line({x, y}, {x + w - 1, y}, c_tl_outer, 1.0f);
            draw_line({x, y}, {x, y + h - 1}, c_tl_outer, 1.0f);
            draw_line({x + w - 1, y}, {x + w - 1, y + h - 1}, c_br_outer, 1.0f);
            draw_line({x, y + h - 1}, {x + w - 1, y + h - 1}, c_br_outer, 1.0f);

            // Inner bevel
            draw_line({x + 1, y + 1}, {x + w - 2, y + 1}, c_tl_inner, 1.0f);
            draw_line({x + 1, y + 1}, {x + 1, y + h - 2}, c_tl_inner, 1.0f);
            draw_line({x + w - 2, y + 1}, {x + w - 2, y + h - 2}, c_br_inner, 1.0f);
            draw_line({x + 1, y + h - 2}, {x + w - 2, y + h - 2}, c_br_inner, 1.0f);
        } else {
            // 1-pixel bevel (Classic / Plasma style)
            auto top_left = sunken ? palette.shadow : palette.light;
            auto bottom_right = sunken ? palette.light : palette.shadow;

            draw_line({rect.x, rect.y}, {rect.x + rect.width - 1, rect.y}, top_left, 1.0f);
            draw_line({rect.x, rect.y}, {rect.x, rect.y + rect.height - 1}, top_left, 1.0f);
            draw_line({rect.x + rect.width - 1, rect.y},
                      {rect.x + rect.width - 1, rect.y + rect.height - 1}, bottom_right, 1.0f);
            draw_line({rect.x, rect.y + rect.height - 1},
                      {rect.x + rect.width - 1, rect.y + rect.height - 1}, bottom_right, 1.0f);
        }
    } else {
        auto bw = style.border_width;
        auto frame_rect = rect;
        if (bottom_shadow) {
            frame_rect.height = std::max(0.0f, frame_rect.height - 1.0f);
        }

        // Clamp so the radius never exceeds half the shortest side — prevents the
        // diamond shape that occurs when corner_radius >= box_size/2 (e.g. GNOME
        // checkbox: 10px radius on a 16px box).
        auto max_r = std::min(frame_rect.width, frame_rect.height) / 2.0f;
        auto cr = std::min(style.corner_radius, max_r);

        if (cr > 0.0f) {
            fill_rounded_rect(frame_rect, bg, cr);
        } else {
            fill_rect(frame_rect, bg);
        }

        if (bw > 0) {
            auto inset = bw / 2.0f;
            auto border_rect = frame_rect.inset(inset);

            if (cr > 0.0f) {
                draw_rounded_rect(border_rect, border, std::max(0.0f, cr - inset), bw);
            } else {
                draw_rect(border_rect, border, bw);
            }
        }

        // Draw the bottom line, with corners, a pixel down, used by Plasma and GNome
        if (bw > 0 && bottom_shadow) {
            auto r = cr;
            // The frame was 1px smaller, so we draw shadow at full size and clip to bottom part
            push_clip({rect.x, rect.y + rect.height - r - 1.0f, rect.width, r + 1.0f});
            draw_rounded_rect(rect.inset(bw / 2.0f), palette.dark_shadow, r, bw);
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

void Painter::draw_text(std::string_view text, Point position, Color const &color, float font_size,
                        FontFamily font, TextOrientation orientation, bool bold, bool italic) {
    if (rasterizer_) {
        rasterizer_->draw_text(*this, text, position, color, font_size, font, orientation, bold,
                               italic);
    }
}

void Painter::draw_mnemonic_text(std::string_view raw_text, Point pos, Color const &color,
                                 float font_size) {
    auto amp_pos = raw_text.find('&');
    if (amp_pos == std::string_view::npos) {
        draw_text(raw_text, pos, color, font_size);
        return;
    }
    auto display = strip_mnemonic(raw_text);
    draw_text(display, pos, color, font_size);
    if (amp_pos >= display.size()) {
        return;
    }
    auto fm = font_metrics(font_size);
    auto ul_y = pos.y + fm.descent * 0.4f;
    auto *plat = detail::current_platform();
    auto *shaper = plat ? plat->shaper() : nullptr;
    if (shaper) {
        auto dir = bidi::detect_base_direction(display);
        auto tl = text::TextLayout(display, dir, *shaper, font_size);
        for (auto const &r : tl.selection_rects(amp_pos, Utf8Iterator::next(display, amp_pos))) {
            draw_line({pos.x + r.x, ul_y}, {pos.x + r.x + r.width, ul_y}, color, 1.0f);
        }
    } else {
        auto before_w =
            amp_pos > 0 ? measure_text(display.substr(0, amp_pos), font_size).width : 0.0f;
        auto ch_w = measure_text(display.substr(amp_pos, 1), font_size).width;
        draw_line({pos.x + before_w, ul_y}, {pos.x + before_w + ch_w, ul_y}, color, 1.0f);
    }
}

Size Painter::measure_text(std::string_view text, float font_size, FontFamily family, bool bold,
                           bool italic) {
    if (!rasterizer_ || text.empty()) {
        return {0, 0};
    }
    return rasterizer_->measure(text, font_size, family, bold, italic);
}

Painter::FontMetrics Painter::font_metrics(float font_size, FontFamily family) {
    if (rasterizer_) {
        return rasterizer_->metrics(font_size, family);
    }
    return {};
}

} // namespace toolkit
