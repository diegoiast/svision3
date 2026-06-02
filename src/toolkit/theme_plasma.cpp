// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_plasma.hpp"
#include "toolkit/painter.hpp"

namespace toolkit {

Plasma6Theme::Plasma6Theme(ColorScheme scheme, std::optional<Palette> p) : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Plasma 6";
    button.padding = {8, 16, 8, 16};
    slider.handle_size = 20.0f;
    slider.groove_thickness = 6.0f;
    focus_ring_margin = 2.0f;
    focus_ring_corner_radius = 4.0f;
    tab_widget.indicator_weight = -2.0f;

    scrollbar.show_buttons = false;
    scrollbar.thickness = 34.0f;
    scrollbar.show_frame = false;
    scrollbar.padding = {0, 4, 0, 4};
}

Palette Plasma6Theme::default_palette(ColorScheme scheme) const {
    Palette p;
    Theme::init_fonts(p);
    Color plasma6_color = Color::from_rgb(0x3daee9);
    p.inline_scrollbars = false;
    p.corner_radius = 6.0f;
    p.tab_radius = 5.0f;
    p.bottom_shadow = true;
    p.chrome_lines = true;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xeff0f1);
        p.window_inactive = Color::from_rgb(0xe3e5e7);
        p.base = Color::from_rgb(0xfcfcfc);
        p.alternate = Color::from_rgb(0xeff0f1);
        p.text = Color::from_rgb(0x232629);
        p.text_disabled = Color::from_argb(0xFF7F8C8D);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = plasma6_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_rgb(0xc7c8c9);
        p.accent = plasma6_color;
        p.link = plasma6_color;
        p.shadow = Color::from_argb(0x22000000);
        p.dark_shadow = Color::from_argb(0x44000000);
        p.background_pressed = Color::from_argb(0xFFE0E0E0);
        p.background_hovered = Color::from_argb(0xFFa3d4fa);
        p.tooltip = Color::from_argb(0xFFFDFDFD);
        p.success = Color::from_argb(0xFF27AE60);
        p.warning = Color::from_argb(0xFFF39C12);
        p.error = Color::from_argb(0xFFE74C3C);
        p.tab_select_background = p.window;
        p.tab_background = Color::from_rgb(0xc7c8c9);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF232629);
        p.base = Color::from_argb(0xFF1B1E20);
        p.alternate = Color::from_argb(0xFF31363B);
        p.text = Color::from_argb(0xFFEFF0F1);
        p.text_disabled = Color::from_argb(0xFF7F8C8D);
        p.placeholder = Color::from_argb(0xFFBDC3C7);
        p.highlight = plasma6_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF3B4045);
        p.accent = plasma6_color;
        p.link = plasma6_color;
        p.shadow = Color::from_argb(0x33000000);
        p.dark_shadow = Color::from_argb(0x55000000);
        p.background_pressed = Color::from_argb(0xFF2C3034);
        p.background_hovered = Color::from_argb(0xFF3B4045);
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.success = Color::from_argb(0xFF27AE60);
        p.warning = Color::from_argb(0xFFF39C12);
        p.error = Color::from_argb(0xFFE74C3C);
        p.tab_select_background = p.window;
        p.tab_background = p.window;
        break;
    }
    return p;
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
