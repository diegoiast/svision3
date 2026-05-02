// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_plasma.hpp"
#include "toolkit/painter.hpp"

namespace toolkit {

Plasma6Theme::Plasma6Theme(Palette p) : BaseTheme(std::move(p)) {
    name = "Plasma 6";
    slider.handle_size = 20.0f;
    slider.groove_thickness = 6.0f;
    focus_ring_margin = 2.0f;
    focus_ring_corner_radius = 4.0f;
    tab_widget.indicator_weight = -4.0f;
}

void Plasma6Theme::draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                  int depth, bool has_children, bool expanded, bool selected,
                                  bool hovered, bool alternate) const {
    auto const &style = tree_view;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto indent = style.indent;

    auto x_offset = style.item_padding_h + depth * indent;

    if (has_children) {
        auto center_x = x_offset + indent / 2;
        auto arrow_y = rect.y + rect.height / 2;
        auto arrow_size = 8.0f;
        auto arrow_offset = arrow_size * 0.3f;

        if (expanded) {
            painter.draw_line({center_x - arrow_size / 2, arrow_y - arrow_offset},
                              {center_x, arrow_y + arrow_size / 2}, palette.text, 1.5f);
            painter.draw_line({center_x, arrow_y + arrow_size / 2},
                              {center_x + arrow_size / 2, arrow_y - arrow_offset}, palette.text,
                              1.5f);
        } else {
            painter.draw_line({center_x - arrow_offset, arrow_y - arrow_size / 2},
                              {center_x + arrow_offset, arrow_y}, palette.text, 1.5f);
            painter.draw_line({center_x - arrow_offset, arrow_y + arrow_size / 2},
                              {center_x + arrow_offset, arrow_y}, palette.text, 1.5f);
        }
    }

    x_offset += indent + 4.0f;

    auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_col = selected ? palette.highlighted_text : palette.text;
    painter.draw_text(text, {x_offset, text_y}, text_col, palette.fonts.size);
}

} // namespace toolkit
