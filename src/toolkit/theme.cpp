// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/types.hpp"
#include <cmath>
#include <spdlog/spdlog.h>

namespace toolkit {

// ── Theme Management ─────────────────────────────────────────────────────────

static std::unique_ptr<Theme> &mutable_current_ptr() {
    static std::unique_ptr<Theme> instance;
    if (!instance) {
        instance = ThemeFactory::create(Theme::detect_system_style());
    }
    return instance;
}

Theme const &Theme::current() { return *mutable_current_ptr(); }

void Theme::set_current(std::unique_ptr<Theme> theme) {
    mutable_current_ptr() = std::move(theme);
}

// ── Palette Initialization ───────────────────────────────────────────────────

static void palette_macos(Palette &p, ColorScheme scheme) {
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
        break;
    }
}

static void palette_win11(Palette &p, ColorScheme scheme) {
    auto windows_blue = Color::from_argb(0xFF0078D4);
    p.tab_radius = 4.0f;
    p.corner_radius = 4.0f;
    p.chrome_lines = false;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xEEF4F9);
        p.window_inactive = Color::from_rgb(0xF3F3F3);
        //p.window = Color::from_argb(0xFFEDEDED);
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
}

static void palette_material(Palette &p, ColorScheme scheme) {
    // default Material 3 primary (Deep Purple)
    Color material_purple = Color::from_rgb(0x6750A4);
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
        break;
    }
}

static void palette_win95(Palette &p, ColorScheme scheme) {
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
        break;
    }
}

static void palette_plasma6(Palette &p, ColorScheme scheme) {
    // default Breeze accent blue
    Color plasma6_color = Color::from_argb(0xFF3DAEE9);
    p.corner_radius = 5.0f;
    p.tab_radius = 5.0f;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFeff0f1);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF7F7F7);
        p.text = Color::from_argb(0xFF2E3436);
        p.text_disabled = Color::from_argb(0xFF7F8C8D);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = plasma6_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFE0E0E0);
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
        break;
    }
}

static void palette_gnome(Palette &p, ColorScheme scheme) {
    p.corner_radius = 8.0f;

    auto adwaita_color = Color::from_argb(0xFF3465A4);

    switch (scheme) {
    case ColorScheme::Light:
        // default Adwaita blue accent

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
        break;
    }
}

Palette Theme::default_palette(ThemeStyle style, ColorScheme scheme) {
    Palette p;
    p.fonts.system = "sans-serif";
    p.fonts.monospace = "monospace";
    p.fonts.size = 14.0f;
    p.fonts.auto_repeat_delay = 0.5f;
    p.fonts.auto_repeat_interval = 0.4f;

    if (auto *plat = detail::current_platform()) {
        auto sf = plat->system_fonts();
        if (!sf.system.empty()) {
            p.fonts.system = sf.system;
        }
        if (!sf.monospace.empty()) {
            p.fonts.monospace = sf.monospace;
        }
        if (sf.size > 0) {
            p.fonts.size = std::floor(sf.size * (96.0f / 72.0f));
        }
        if (sf.auto_repeat_delay > 0) {
            p.fonts.auto_repeat_delay = sf.auto_repeat_delay;
        }
        if (sf.auto_repeat_interval > 0) {
            p.fonts.auto_repeat_interval = sf.auto_repeat_interval;
        }
    }

    switch (style) {
    case ThemeStyle::MacOS:
        palette_macos(p, scheme);
        break;
    case ThemeStyle::Material:
        palette_material(p, scheme);
        break;
    case ThemeStyle::Win11:
        palette_win11(p, scheme);
        break;
    case ThemeStyle::Win95:
        palette_win95(p, scheme);
        break;
    case ThemeStyle::Plasma6:
        palette_plasma6(p, scheme);
        break;
    case ThemeStyle::GNOME:
        palette_gnome(p, scheme);
        break;
    default:
        palette_material(p, scheme);
        break;
    }

    p.auto_repeat_delay = p.fonts.auto_repeat_delay;
    p.auto_repeat_interval = p.fonts.auto_repeat_interval;
    return p;
}

void Theme::draw_focus_ring_for_widget(Painter &painter, Widget const *widget) const {
    if (!widget) {
        return;
    }

    auto global_x = 0.0f;
    auto global_y = 0.0f;
    Widget const *w = widget;
    while (w) {
        global_x += w->rect().x;
        global_y += w->rect().y;
        w = w->parent();
    }

    auto margin = focus_ring_margin;
    auto r = Rect{global_x - margin, global_y - margin, widget->rect().width + margin * 2,
                  widget->rect().height + margin * 2};
    auto corner_radius = palette.corner_radius + focus_ring_corner_radius;

    painter.set_line_style(focus_ring_line_style);
    draw_focus_ring(painter, r, corner_radius);
    painter.set_line_style(Painter::LineStyle::Solid);
}

const char *Theme::style_name(ThemeStyle style) {
    switch (style) {
    case ThemeStyle::MacOS:
        return "macOS";
    case ThemeStyle::Material:
        return "Material";
    case ThemeStyle::Win11:
        return "Windows 11";
    case ThemeStyle::Win95:
        return "Windows 95";
    case ThemeStyle::Plasma6:
        return "Plasma 6";
    case ThemeStyle::GNOME:
        return "GNOME";
    }
    return "Unknown";
}

ThemeStyle Theme::detect_system_style() {
#if defined(__APPLE__)
    return ThemeStyle::MacOS;
#elif defined(_WIN32)
    return ThemeStyle::Win11;
#else
    const char *xdg = std::getenv("XDG_CURRENT_DESKTOP");
    if (xdg) {
        std::string s(xdg);
        auto contains = [&](std::string_view sub) {
            auto it = std::search(s.begin(), s.end(), sub.begin(), sub.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            });
            return it != s.end();
        };

        if (contains("GNOME")) {
            return ThemeStyle::GNOME;
        }
        if (contains("KDE") || contains("PLASMA")) {
            return ThemeStyle::Plasma6;
        }
    }
    return ThemeStyle::Material;
#endif
}

} // namespace toolkit
