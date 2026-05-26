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

MaterialTheme::MaterialTheme(ColorScheme scheme, std::optional<Palette> p) : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Material";
    menu.padding = {4, 4, 4, 4};
    menubar.padding = {4, 12, 4, 12};
    slider.handle_size = 18.0f;
    slider.groove_thickness = 4.0f;
    focus_ring_margin = 3.0f;
    focus_ring_corner_radius = 3.0f;
    tab_widget.indicator_weight = 2.0f;
}

Palette MaterialTheme::default_palette(ColorScheme scheme) const {
    auto material_purple = Color::from_rgb(0x6750A4);

    Palette p;
    Theme::init_fonts(p);
    p.corner_radius = 4.0f;
    p.tab_radius = 0.0f;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xFFFBFE);
        p.base = Color::from_rgb(0xFFFFFF);
        p.alternate = Color::from_rgb(0xF7F2FA);
        p.text = Color::from_rgb(0x1C1B1F);
        p.text_disabled = Color::from_rgb(0x9E9E9E);
        p.placeholder = Color::from_rgb(0x79747E);
        p.highlight = material_purple;
        p.highlighted_text = Color::from_rgb(0x000000);
        p.border = Color::from_rgb(0xE7E0EC);
        p.accent = material_purple;
        p.link = Color::from_rgb(0xFF2962FF);
        p.shadow = Color::from_rgb(0x000000);
        p.dark_shadow = Color::from_rgb(0x000000);
        p.background_pressed = Color::from_rgb(0xE8DEF8);
        p.background_hovered = Color::from_rgb(0xF3EDF7);
        p.tooltip = Color::from_rgb(0xE6E1E5);
        p.success = Color::from_rgb(0x2E7D32);
        p.warning = Color::from_rgb(0xF9A825);
        p.error = Color::from_rgb(0xB3261E);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_rgb(0x1C1B1F);
        p.base = Color::from_rgb(0x1C1B1F);
        p.alternate = Color::from_rgb(0x292529);
        p.text = Color::from_rgb(0xE6E1E5);
        p.text_disabled = Color::from_rgb(0x9E9E9E);
        p.placeholder = Color::from_rgb(0x79747E);
        p.highlight = material_purple;
        p.highlighted_text = Color::from_rgb(0xFFFFFF);
        p.border = Color::from_rgb(0x49454F);
        p.accent = material_purple;
        p.link = Color::from_rgb(0x82B1FF);
        p.shadow = Color::from_rgb(0x000000);
        p.dark_shadow = Color::from_argb(0x66000000);
        p.background_pressed = Color::from_rgb(0x3E3748);
        p.background_hovered = Color::from_rgb(0x4A4256);
        p.tooltip = Color::from_rgb(0x2B2B2B);
        p.success = Color::from_rgb(0x2E7D32);
        p.warning = Color::from_rgb(0xF9A825);
        p.error = Color::from_rgb(0xB3261E);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
        break;
    }
    return p;
}

GnomeTheme::GnomeTheme(ColorScheme scheme, std::optional<Palette> p) : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "GNOME";
    bool dark = palette.window.luma() < 0.5f;

    slider.handle_size = 22.0f;
    slider.groove_thickness = 6.0f;

    focus_ring_margin = 2.0f;
    focus_ring_corner_radius = 2.0f;
}

Palette GnomeTheme::default_palette(ColorScheme scheme) const {
    auto adwaita_color = Color::from_argb(0xFF3465A4);

    Palette p;
    Theme::init_fonts(p);
    p.corner_radius = 6.0f;
    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFFAFAFA);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF0F0F0);
        p.text = Color::from_argb(0xFF2E3436);
        p.text_disabled = Color::from_argb(0xFF888A85);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = adwaita_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFE0E0E0);
        p.accent = adwaita_color;
        p.link = adwaita_color;
        p.shadow = Color::from_argb(0x22000000);
        p.dark_shadow = Color::from_argb(0x44000000);
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.background_pressed = Color::from_argb(0xFFECECEC);
        p.background_hovered = Color::from_argb(0xFFF5F5F5);
        p.success = Color::from_argb(0xFF2E7D32);
        p.warning = Color::from_argb(0xFFFBC02D);
        p.error = Color::from_argb(0xFFC62828);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF2E3436);
        p.base = Color::from_argb(0xFF3B3B3B);
        p.alternate = Color::from_argb(0xFF4A4A4A);
        p.text = Color::from_argb(0xFFECECEC);
        p.text_disabled = Color::from_argb(0xFF888A85);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = adwaita_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF555555);
        p.accent = adwaita_color;
        p.link = adwaita_color;
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.background_pressed = Color::from_argb(0xFF484848);
        p.background_hovered = Color::from_argb(0xFF565656);
        p.success = Color::from_argb(0xFF2E7D32);
        p.warning = Color::from_argb(0xFFFBC02D);
        p.error = Color::from_argb(0xFFC62828);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
        break;
    }
    return p;
}

namespace ThemeFactory {
std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme) {
    switch (style) {
    case ThemeStyle::System: {
        auto s = Theme::detect_system_style();
        return create(s, scheme);
    }
    case ThemeStyle::MacOS:
        return std::make_unique<MacOSTheme>(scheme);
    case ThemeStyle::Material:
        return std::make_unique<MaterialTheme>(scheme);
    case ThemeStyle::Win11:
        return std::make_unique<Win11Theme>(scheme);
    case ThemeStyle::Win95:
        return std::make_unique<Win95Theme>(scheme);
    case ThemeStyle::Plasma6:
        return std::make_unique<Plasma6Theme>(scheme);
    case ThemeStyle::GNOME:
        return std::make_unique<GnomeTheme>(scheme);
    }

    return std::make_unique<MaterialTheme>(scheme);
}
} // namespace ThemeFactory

} // namespace toolkit
