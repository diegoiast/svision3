// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_factory.hpp"
#include "toolkit/theme_base.hpp"
#include "toolkit/theme_plasma.hpp"
#include "toolkit/theme_macos.hpp"
#include "toolkit/theme_win11.hpp"
#include "toolkit/theme_win95.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"

namespace toolkit {

class MaterialTheme : public BaseTheme {
  public:
    explicit MaterialTheme(Palette p) : BaseTheme(std::move(p)) {
        name = "Material";
        button.padding = {10, 24, 10, 24};
        menu.padding = {4, 4, 4, 4};
        menubar.padding = {4, 12, 4, 12};
        slider.handle_size = 18.0f;
        slider.groove_thickness = 4.0f;
        focus_ring_margin = 3.0f;
        focus_ring_corner_radius = 3.0f;
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto const &style = tab_widget;
        auto bg = palette.window;
        if (active) {
            bg = palette.base;
            if (palette.background_pressed) {
                bg = *palette.background_pressed;
            }
        } else {
            if (hovered && palette.background_hovered) {
                bg = *palette.background_hovered;
            }
        }

        auto text_c = active ? palette.highlighted_text : palette.text;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_w = painter.text_size(text, palette.fonts.size).width;
        auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
        auto left_space = style.tab_padding_h;
        auto text_area_w = rect.width - left_space - right_space;
        auto text_x = rect.x + left_space + (text_area_w - text_w) / 2.0f;
        if (text_x < rect.x + left_space) {
            text_x = rect.x + left_space;
        }

        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);

        if (has_close) {
            auto close_btn_size = 14.0f;
            auto close_x = rect.x + rect.width - style.tab_padding_h - close_btn_size;
            auto close_rect = Rect{close_x, rect.y + (rect.height - close_btn_size) / 2.0f,
                                   close_btn_size, close_btn_size};
            auto close_cy = rect.y + rect.height / 2.0f;
            auto close_cx = close_x + close_btn_size / 2.0f;

            if (hovered_close) {
                painter.fill_rounded_rect(close_rect, Color::rgb(0.9f, 0.2f, 0.2f), 4.0f);
            }

            auto cs = close_btn_size * 0.3f;
            auto x_col = hovered_close ? Color::rgb(1.0f, 1.0f, 1.0f)
                                       : Color::rgba(text_c.r, text_c.g, text_c.b, 0.6f);
            painter.draw_line({close_cx - cs, close_cy - cs}, {close_cx + cs, close_cy + cs}, x_col,
                              1.5f);
            painter.draw_line({close_cx + cs, close_cy - cs}, {close_cx - cs, close_cy + cs}, x_col,
                              1.5f);
        }

        if (active) {
            auto indicator = Rect{};
            float lw = 2.0f;
            if (orientation == TabOrientation::North) {
                indicator = {rect.x, rect.y + rect.height - lw, rect.width, lw};
            } else if (orientation == TabOrientation::South) {
                indicator = {rect.x, rect.y, rect.width, lw};
            } else if (orientation == TabOrientation::West) {
                indicator = {rect.x + rect.width - lw, rect.y, lw, rect.height};
            } else if (orientation == TabOrientation::East) {
                indicator = {rect.x, rect.y, lw, rect.height};
            }
            painter.fill_rect(indicator, palette.accent);
        }
    }
};

class GnomeTheme : public BaseTheme {
  public:
    explicit GnomeTheme(Palette p) : BaseTheme(std::move(p)) {
        name = "GNOME";
        bool dark = palette.window.luma() < 0.5f;
        Color btn_bg = dark ? palette.window.lighten(0.04f) : palette.window.darken(0.03f);
        button.padding = {8, 20, 8, 20};

        slider.handle_size = 22.0f;
        slider.groove_thickness = 6.0f;

        focus_ring_margin = 2.0f;
        focus_ring_corner_radius = 2.0f;
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto const &style = tab_widget;
        // FIXME: we do not support hover colors. Should we use the button colors?
        auto bg = palette.window;
        if (active) {
            if (palette.background_pressed) {
                bg = *palette.background_pressed;
            }
        } else {
            if (hovered && palette.background_hovered) {
                bg = *palette.background_hovered;
            }
        }
        auto text_c = active ? palette.highlighted_text : palette.text;
        auto tab_rect = rect.inset(2.0f);
        painter.fill_rounded_rect(tab_rect, bg, 6.0f);

        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_w = painter.text_size(text, palette.fonts.size).width;
        auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
        auto left_space = style.tab_padding_h;
        auto text_area_w = rect.width - left_space - right_space;
        auto text_x = rect.x + left_space + (text_area_w - text_w) / 2.0f;
        if (text_x < rect.x + left_space) {
            text_x = rect.x + left_space;
        }

        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);

        if (has_close) {
            // FIXME: clsoe button size is hardcoded
            auto close_btn_size = 14.0f;
            auto close_x = rect.x + rect.width - style.tab_padding_h - close_btn_size - 2.0f;
            auto close_cy = rect.y + rect.height / 2.0f;
            auto close_cx = close_x + close_btn_size / 2.0f;

            if (hovered_close) {
                painter.fill_circle({close_cx, close_cy}, close_btn_size / 2.0f + 2.0f,
                                    Color::rgba(text_c.r, text_c.g, text_c.b, 0.15f));
            }

            auto cs = close_btn_size * 0.3f;
            auto x_col = Color::rgba(text_c.r, text_c.g, text_c.b, 0.7f);
            painter.draw_line({close_cx - cs, close_cy - cs}, {close_cx + cs, close_cy + cs}, x_col,
                              1.5f);
            painter.draw_line({close_cx + cs, close_cy - cs}, {close_cx - cs, close_cy + cs}, x_col,
                              1.5f);
        }
    }
};

std::unique_ptr<Theme> ThemeFactory::create(ThemeStyle style, ColorScheme scheme) {
    return create(style, Theme::default_palette(style, scheme));
}

std::unique_ptr<Theme> ThemeFactory::create(ThemeStyle style, Palette const &palette) {
    std::unique_ptr<Theme> t;
    switch (style) {
    case ThemeStyle::MacOS:
        t = std::make_unique<MacOSTheme>(palette);
        break;
    case ThemeStyle::Material:
        t = std::make_unique<MaterialTheme>(palette);
        break;
    case ThemeStyle::Win11:
        t = std::make_unique<Win11Theme>(palette);
        break;
    case ThemeStyle::Win95:
        t = std::make_unique<Win95Theme>(palette);
        break;
    case ThemeStyle::Plasma6:
        t = std::make_unique<Plasma6Theme>(palette);
        break;
    case ThemeStyle::GNOME:
        t = std::make_unique<GnomeTheme>(palette);
        break;
    default:
        t = std::make_unique<BaseTheme>(palette);
        break;
    }

    t->name = Theme::style_name(style);
    t->style = style;
    t->palette = palette;
    return t;
}

} // namespace toolkit
