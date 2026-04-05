// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_win11.hpp"
#include "toolkit/painter.hpp"

namespace toolkit {

Win11Theme::Win11Theme(Palette p) : BaseTheme(std::move(p)) {
    name = "Windows 11";
    focus_ring_margin = 3.0f;
    focus_ring_corner_radius = 4.0f;
}

void Win11Theme::draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title,
                                   bool hovered, bool active, bool show_mnemonics,
                                   int mnemonic_index) const {
    auto const &style = menubar;
    auto padding = style.padding;
    auto fm = painter.font_metrics(palette.fonts.size);

    if (hovered || active) {
        Color bg = palette.highlight;
        auto hover_rect = rect.inset(2.0f);
        painter.fill_rounded_rect(hover_rect, bg, palette.corner_radius);
    }

    auto baseline = (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_c = palette.text;
    if (hovered || active) {
        text_c = palette.highlighted_text;
    }
    painter.draw_text(title, {rect.x + padding.left, baseline}, text_c, palette.fonts.size);
}

void Win11Theme::draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                          bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                          bool hovered_close) const {
    BaseTheme::draw_tab(painter, rect, text, active, hovered, enabled, orientation, has_close,
                        hovered_close);

    if (active) {
        auto indicator = Rect{};
        auto lw = 2.0f;
        if (orientation == TabOrientation::North) {
            indicator = {rect.x + 4.0f, rect.y + rect.height - lw, rect.width - 8.0f, lw};
        } else if (orientation == TabOrientation::South) {
            indicator = {rect.x + 4.0f, rect.y, rect.width - 8.0f, lw};
        } else if (orientation == TabOrientation::West) {
            indicator = {rect.x + rect.width - lw, rect.y + 4.0f, lw, rect.height - 8.0f};
        } else if (orientation == TabOrientation::East) {
            indicator = {rect.x, rect.y, lw, rect.height};
        }
        painter.fill_rect(indicator, palette.accent);
    }
}

void Win11Theme::draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                int depth, bool has_children, bool expanded, bool selected,
                                bool hovered, bool alternate) const {
    auto const &style = tree_view;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto indent = style.indent;

    auto x_offset = style.item_padding_h + depth * indent;

    if (has_children) {
        auto arrow_x = x_offset + 4;
        auto arrow_y = rect.y + rect.height / 2;
        auto arrow_size = 8.0f;

        if (expanded) {
            painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                  {arrow_x + arrow_size, arrow_y},
                                  {arrow_x, arrow_y + arrow_size / 2}, palette.text);
        } else {
            painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                  {arrow_x + arrow_size, arrow_y - arrow_size / 2},
                                  {arrow_x + arrow_size / 2, arrow_y + arrow_size / 2},
                                  palette.text);
        }
        x_offset += indent;
    }

    auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_col = selected ? palette.accent : palette.text;
    painter.draw_text(text, {x_offset, text_y}, text_col, palette.fonts.size);
}

} // namespace toolkit
