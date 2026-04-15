// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_win11.hpp"
#include "toolkit/painter.hpp"

namespace toolkit {

Win11Theme::Win11Theme(Palette p) : BaseTheme(std::move(p)) {
    name = "Windows 11";
    focus_ring_margin = 3.0f;
    focus_ring_corner_radius = 4.0f;
    tab_widget.indicator_weight = 2.0f;
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
