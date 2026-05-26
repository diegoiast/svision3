// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_macos.hpp"
#include "toolkit/painter.hpp"

namespace toolkit {

MacOSTheme::MacOSTheme(ColorScheme scheme, std::optional<Palette> p) : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "macOS";
    focus_ring_margin = 2.0f;
    focus_ring_corner_radius = 4.0f;
}

Palette MacOSTheme::default_palette(ColorScheme scheme) const {
    Palette p;
    Theme::init_fonts(p);
    auto macBlue = Color::from_argb(0xFF0A84FF);
    p.border_width = 0.5f;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFF2F2F7);
        p.window_inactive = Color::from_argb(0xFFF3F3F3);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF9F9FB);
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFF8E8E93);
        p.placeholder = Color::from_argb(0xFFAEAEB2);
        p.highlight = macBlue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFD1D1D6);
        p.accent = macBlue;
        p.link = macBlue;
        p.shadow = Color::from_argb(0x33000000);
        p.dark_shadow = Color::from_argb(0x55000000);
        p.background_pressed = Color::from_argb(0xFFE5E5EA);
        p.background_hovered = Color::from_argb(0xFFEDEDF0);
        p.tooltip = Color::from_argb(0xF2F2F2F2);
        p.success = Color::from_argb(0xFF34C759);
        p.warning = Color::from_argb(0xFFFF9F0A);
        p.error = Color::from_argb(0xFFFF3B30);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF1E1E1E);
        p.base = Color::from_argb(0xFF2C2C2E);
        p.alternate = Color::from_argb(0xFF3A3A3C);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF8E8E93);
        p.placeholder = Color::from_argb(0xFF636366);
        p.highlight = macBlue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF3A3A3C);
        p.accent = macBlue;
        p.link = macBlue;
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::from_argb(0xE62C2C2E);
        p.background_pressed = Color::from_argb(0xFF3A3A3C);
        p.background_hovered = Color::from_argb(0xFF48484A);
        p.success = Color::from_argb(0xFF30D158);
        p.warning = Color::from_argb(0xFFFF9F0A);
        p.error = Color::from_argb(0xFFFF453A);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
        break;
    }
    return p;
}

} // namespace toolkit
