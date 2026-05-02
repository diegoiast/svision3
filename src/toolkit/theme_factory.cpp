// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_factory.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme_base.hpp"
#include "toolkit/theme_macos.hpp"
#include "toolkit/theme_plasma.hpp"
#include "toolkit/theme_win11.hpp"
#include "toolkit/theme_win95.hpp"
#include "toolkit/types.hpp"

namespace toolkit {

class MaterialTheme : public BaseTheme {
  public:
    explicit MaterialTheme(Palette p) : BaseTheme(std::move(p)) {
        name = "Material";
        menu.padding = {4, 4, 4, 4};
        menubar.padding = {4, 12, 4, 12};
        slider.handle_size = 18.0f;
        slider.groove_thickness = 4.0f;
        focus_ring_margin = 3.0f;
        focus_ring_corner_radius = 3.0f;
        tab_widget.indicator_weight = 2.0f;
    }
};

class GnomeTheme : public BaseTheme {
  public:
    explicit GnomeTheme(Palette p) : BaseTheme(std::move(p)) {
        name = "GNOME";
        bool dark = palette.window.luma() < 0.5f;
        Color btn_bg = dark ? palette.window.lighten(0.04f) : palette.window.darken(0.03f);

        slider.handle_size = 22.0f;
        slider.groove_thickness = 6.0f;

        focus_ring_margin = 2.0f;
        focus_ring_corner_radius = 2.0f;
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto old_radius = palette.tab_radius;
        const_cast<Palette &>(palette).tab_radius = 6.0f;
        BaseTheme::draw_tab(painter, rect.inset(2.0f), text, active, hovered, enabled, orientation,
                            has_close, hovered_close);
        const_cast<Palette &>(palette).tab_radius = old_radius;
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
