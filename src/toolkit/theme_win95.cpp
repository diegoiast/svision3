// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_win95.hpp"
#include "toolkit/painter.hpp"
#include <algorithm>
#include <cmath>

namespace toolkit {

Win95Theme::Win95Theme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Windows 95";
    focus_ring_margin = 0.0f;
    focus_ring_corner_radius = 0.0f;
    focus_ring_line_style = Painter::LineStyle::Dotted;
}

Palette Win95Theme::default_palette(ColorScheme scheme) const {
    Palette p;
    Theme::init_fonts(p);
    Color windows95_color = Color::from_argb(0xFF000080);
    p.beveled = true;
    p.progress_bar_height = 20;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFC0C0C0);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFC0C0C0);
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFF808080);
        p.placeholder = Color::from_argb(0xFF808080);
        p.highlight = windows95_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF808080);
        p.accent = windows95_color;
        p.link = windows95_color;
        p.shadow = Color::from_argb(0xFF404040);
        p.dark_shadow = Color::from_argb(0xFF000000);
        p.background_pressed = Color::from_argb(0xFFB0B0B0);
        p.background_hovered = Color::from_argb(0xFFB8B8B8);
        p.tooltip = Color::from_argb(0xFFFFFFE1);
        p.success = Color::from_argb(0xFF008000);
        p.warning = Color::from_argb(0xFFFF8000);
        p.error = Color::from_argb(0xFF800000);
        p.tab_select_background = p.window;
        p.tab_background = p.window;
        p.tab_radius = 0.0f;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF000000);
        p.base = Color::from_argb(0xFF202020);
        p.alternate = Color::from_argb(0xFF303030);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF808080);
        p.placeholder = Color::from_argb(0xFF808080);
        p.highlight = windows95_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF404040);
        p.accent = windows95_color;
        p.link = windows95_color;
        p.shadow = Color::from_argb(0xFF000000);
        p.dark_shadow = Color::from_argb(0xFF000000);
        p.background_pressed = windows95_color;
        p.background_hovered = Color::from_argb(0xFF303030);
        p.tooltip = Color::from_argb(0xFFFFFFE1);
        p.success = Color::from_argb(0xFF008000);
        p.warning = Color::from_argb(0xFFFF8000);
        p.error = Color::from_argb(0xFF800000);
        p.tab_select_background = p.window;
        p.tab_background = p.window;
        p.tab_radius = 0.0f;
        break;
    }
    return p;
}

void Win95Theme::draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const {
    auto lw = 2.0f;
    auto dash_len = 2.0f;
    auto gap_len = 2.0f;
    auto x = rect.x;
    auto y = rect.y;
    auto w = rect.width;
    auto h = rect.height;
    auto ring = palette.border;
    ring.a = 0.5f;

    painter.draw_rect(rect, ring, lw);
    auto draw_dashed_line = [&](Point start, Point end) {
        auto dx = end.x - start.x;
        auto dy = end.y - start.y;
        auto len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) {
            return;
        }
        auto ux = dx / len;
        auto uy = dy / len;
        auto pos = 0.0f;
        auto drawing = true;
        while (pos < len) {
            auto seg_len = drawing ? dash_len : gap_len;
            auto next_pos = std::min(pos + seg_len, len);
            if (drawing) {
                painter.draw_line({start.x + ux * pos, start.y + uy * pos},
                                  {start.x + ux * next_pos, start.y + uy * next_pos}, ring, lw);
            }
            pos = next_pos;
            drawing = !drawing;
        }
    };

    if (corner_radius > 0.0f) {
        auto cr = std::max(0.0f, corner_radius);
        draw_dashed_line({x + cr, y}, {x + w - cr, y});
        draw_dashed_line({x + w, y + cr}, {x + w, y + h - cr});
        draw_dashed_line({x + w - cr, y + h}, {x + cr, y + h});
        draw_dashed_line({x, y + h - cr}, {x, y + cr});

        // Connect corners with diagonals
        draw_dashed_line({x + cr, y}, {x, y + cr});
        draw_dashed_line({x + w - cr, y}, {x + w, y + cr});
        draw_dashed_line({x + w, y + h - cr}, {x + w - cr, y + h});
        draw_dashed_line({x + cr, y + h}, {x, y + h - cr});
    } else {
        draw_dashed_line({x, y}, {x + w, y});
        draw_dashed_line({x + w, y}, {x + w, y + h});
        draw_dashed_line({x + w, y + h}, {x, y + h});
        draw_dashed_line({x, y + h}, {x, y});
    }
}

void Win95Theme::draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                int depth, bool has_children, bool expanded, bool selected,
                                bool hovered, bool alternate) const {
    auto const &style = tree_view;
    auto const &cb_style = checkbox;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto x_offset = style.item_padding_h + depth * style.indent;

    if (has_children) {
        auto icon_x = x_offset;
        auto box_size = cb_style.box_size;
        auto box_rect = Rect{icon_x, rect.y + (rect.height - box_size) / 2.0f, box_size, box_size};

        painter.draw_filled_frame(box_rect, palette.base, palette.border, palette, false);

        auto expand_collapse_char = expanded ? "-" : "+";
        auto char_w = painter.measure_text(expand_collapse_char, palette.fonts.size).width;
        auto char_x = icon_x + (box_size - char_w) / 2.0f;
        auto char_y = box_rect.y + (box_size - fm.height) / 2.0f + fm.ascent;

        painter.draw_text(expand_collapse_char, Point{char_x, char_y}, palette.text,
                          palette.fonts.size);
    }

    x_offset += style.indent + 4.0f;

    auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_col = selected ? palette.highlighted_text : palette.text;
    painter.draw_text(text, Point{x_offset, text_y}, text_col, palette.fonts.size);
}

void Win95Theme::draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                                   WidgetState const &state) const {
    auto const chunk_width = 8.0f;
    auto const chunk_gap = 2.0f;
    auto enabled = state.enabled;

    auto bg = enabled ? palette.window : palette.window.darken(0.1f);
    auto fill_c = enabled ? palette.accent : palette.accent.darken(0.2f);
    auto inner = rect.inset(palette.border_width);
    auto chunk_count = static_cast<int>(inner.width / (chunk_width + chunk_gap));
    auto fill_w = inner.width * std::clamp(progress, 0.0f, 1.0f);

    for (int i = 0; i < chunk_count; ++i) {
        auto cx = inner.x + i * (chunk_width + chunk_gap);
        if (cx + chunk_width > inner.x + fill_w) {
            break;
        }
        painter.fill_rect({cx, inner.y, chunk_width, inner.height}, fill_c);
    }
}

} // namespace toolkit
