// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_win11.hpp"
#include "toolkit/painter.hpp"

namespace toolkit {

Win11Theme::Win11Theme(ColorScheme scheme, std::optional<Palette> p) : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Windows 11";
    focus_ring_margin = 3.0f;
    focus_ring_corner_radius = 4.0f;
    palette.corner_radius = 9.0f;

    button.padding = {5, 20, 5, 20};
    tab_widget.indicator_weight = {};
    radio.accent_fill = true;
}

Palette Win11Theme::default_palette(ColorScheme scheme) const {
    Palette p;
    Theme::init_fonts(p);
    auto windows_blue = Color::from_argb(0xFF0078D4);
    p.tab_radius = 4.0f;
    p.corner_radius = 4.0f;
    p.chrome_lines = false;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xEEF4F9);
        p.window_inactive = Color::from_rgb(0xF3F3F3);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF9F9F9);
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFF6D6D6D);
        p.placeholder = Color::from_argb(0xFF8A8A8A);
        p.highlight = windows_blue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFDCDCDC);
        p.accent = windows_blue;
        p.link = Color::from_argb(0xFF0067C0);
        p.shadow = Color::from_argb(0x20000000);
        p.dark_shadow = Color::from_argb(0x40000000);
        p.tooltip = Color::from_argb(0xFFFFFFFF);
        p.background_pressed = Color::from_argb(0xFFE5E5E5);
        p.background_hovered = Color::from_argb(0xFFEDEDED);
        p.success = Color::from_argb(0xFF107C10);
        p.warning = Color::from_argb(0xFFFFB900);
        p.error = Color::from_argb(0xFFD13438);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF202020);
        p.base = Color::from_argb(0xFF2B2B2B);
        p.alternate = Color::from_argb(0xFF333333);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF8A8A8A);
        p.placeholder = Color::from_argb(0xFF6D6D6D);
        p.highlight = windows_blue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF3C3C3C);
        p.accent = windows_blue;
        p.link = Color::from_argb(0xFF4CC2FF);
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::from_argb(0xFF2B2B2B);
        p.background_pressed = Color::from_argb(0xFF3A3A3A);
        p.background_hovered = Color::from_argb(0xFF444444);
        p.success = Color::from_argb(0xFF6CCB5F);
        p.warning = Color::from_argb(0xFFFFC83D);
        p.error = Color::from_argb(0xFFFF5F5F);
        break;
    }
    return p;
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

void Win11Theme::draw_tab_content_background(Painter &painter, Rect const &rect) const {
    painter.fill_rect(rect, palette.base);
}

} // namespace toolkit
