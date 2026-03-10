// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme.hpp"
#include "toolkit/platform.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace toolkit {

static Theme &mutable_current() {
    static Theme instance = Theme::create(Theme::detect_system_style());
    return instance;
}

Theme const &Theme::current() { return mutable_current(); }

void Theme::set_current(Theme theme) { mutable_current() = std::move(theme); }

// FIXME: this should move to the color struct
static Color gray(float v) { return Color::rgb(v, v, v); }

// FIXME: this should move to the color struct
static float luma(Color c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

// FIXME: this should move to the color struct, with a multiplier (not only 0.5)
static Color mid(Color a, Color b) {
    return Color::rgb((a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2);
}

// FIXME: this should move to the color struct, with a multiplier
static Color blend(Color a, Color b, float t) {
    return Color::rgb(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t);
}

// ── Base derivation from palette ─────────────────────────────────────────────

static void apply_base(WidgetStyle &ws, Palette const &p) {
    ws.background          = p.widget_bg;
    ws.border              = p.border;
    ws.border_focused      = p.border;
    ws.text                = p.text;
    ws.background_selected = p.background_selected;
    ws.highlight = p.highlight;
    ws.shadow = p.shadow;
    ws.border_width = p.border_width;
    ws.corner_radius = p.corner_radius;
    ws.font_size = p.font_size;
    ws.beveled = p.beveled;
}

// ── Default palettes ─────────────────────────────────────────────────────────

static void palette_macos(Palette &p, ColorScheme scheme) {
    p.corner_radius = 6.0f;
    p.border_width = 0.5f;
    Color macBlue = Color::rgb(0.0f, 0.48f, 1.0f);
    p.accent = macBlue;
    p.background_selected = macBlue;

    switch (scheme) {
    case ColorScheme::Light:
        p.window_bg = gray(0.93f);
        p.widget_bg = gray(1.0f);
        p.input_bg = gray(1.0f);
        p.text = gray(0.20f);
        p.border = gray(0.75f);
        p.alternate_bg = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window_bg = gray(0.18f);
        p.widget_bg = gray(0.24f);
        p.input_bg = gray(0.24f);
        p.text = gray(0.92f);
        p.border = gray(0.38f);
        p.alternate_bg = gray(0.28f);
        break;
    }
}

static void palette_material(Palette &p, ColorScheme scheme) {
    p.corner_radius = 4.0f;
    Color matPurple = Color::rgb(0.384f, 0.0f, 0.933f);
    p.accent = matPurple;
    p.background_selected = matPurple;

    switch (scheme) {
    case ColorScheme::Light:
        p.window_bg = gray(0.98f);
        p.widget_bg = gray(1.0f);
        p.input_bg = gray(1.0f);
        p.text = gray(0.13f);
        p.border = gray(0.74f);
        p.alternate_bg = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window_bg = Color::rgb(0.07f, 0.07f, 0.07f);
        p.widget_bg = Color::rgb(0.12f, 0.12f, 0.12f);
        p.input_bg = Color::rgb(0.12f, 0.12f, 0.12f);
        p.text = gray(0.93f);
        p.border = gray(0.33f);
        p.accent = Color::rgb(0.55f, 0.33f, 0.97f);
        p.background_selected = p.accent;
        p.alternate_bg = Color::rgb(0.16f, 0.16f, 0.16f);
        break;
    }
}

static void palette_win11(Palette &p, ColorScheme scheme) {
    p.corner_radius = 4.0f;
    Color winBlue = Color::rgb(0.0f, 0.47f, 0.84f);
    p.accent = winBlue;
    p.background_selected = winBlue;

    switch (scheme) {
    case ColorScheme::Light:
        p.window_bg = gray(0.95f);
        p.widget_bg = gray(1.0f);
        p.input_bg = gray(1.0f);
        p.text = gray(0.10f);
        p.border = gray(0.68f);
        p.alternate_bg = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window_bg = Color::rgb(0.13f, 0.13f, 0.13f);
        p.widget_bg = Color::rgb(0.18f, 0.18f, 0.18f);
        p.input_bg = Color::rgb(0.18f, 0.18f, 0.18f);
        p.text = gray(0.95f);
        p.border = gray(0.30f);
        p.background_selected = winBlue;
        p.alternate_bg = Color::rgb(0.22f, 0.22f, 0.22f);
        break;
    }
}

static void palette_win95(Palette &p, ColorScheme scheme) {
    p.beveled = true;
    switch (scheme) {
    case ColorScheme::Light:
        p.window_bg = gray(0.75f);
        p.widget_bg = gray(0.75f);
        p.input_bg = gray(1.0f);
        p.text = gray(0.0f);
        p.border = gray(0.0f);
        p.accent = Color::rgb(0.0f, 0.0f, 0.5f);
        p.background_selected = p.accent;
        p.highlight = gray(1.0f);
        p.shadow = gray(0.50f);
        p.alternate_bg = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window_bg = gray(0.25f);
        p.widget_bg = gray(0.30f);
        p.input_bg = gray(0.10f);
        p.text = gray(0.90f);
        p.border = gray(0.10f);
        p.accent = Color::rgb(0.0f, 0.0f, 0.8f);
        p.background_selected = p.accent;
        p.highlight = gray(0.45f);
        p.shadow = gray(0.12f);
        p.alternate_bg = gray(0.35f);
        break;
    }
}

static void palette_plasma6(Palette &p, ColorScheme scheme) {
    p.corner_radius = 5.0f;
    Color roseLt = Color::rgb(0.93f, 0.40f, 0.58f);
    switch (scheme) {
    case ColorScheme::Light:
        // Window background: 0xeff0f1
        // Active border, selected list: 0x3daee9
        // active button: 0xd6ecf8
        p.window_bg = Color::from_argb(0xFFeff0f1);
        p.widget_bg = gray(1.0f);
        p.input_bg = gray(1.0f);
        p.text = Color::rgb(0.137f, 0.149f, 0.161f);
        p.border = Color::rgb(0.737f, 0.753f, 0.773f);
        p.accent = Color::from_argb(0xFF3daee9);
        p.background_selected = Color::from_argb(0xFFd6ecf8);
        p.alternate_bg = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window_bg = Color::rgb(0.137f, 0.149f, 0.161f);
        p.widget_bg = Color::rgb(0.192f, 0.212f, 0.231f);
        p.input_bg = Color::rgb(0.192f, 0.212f, 0.231f);
        p.text = Color::rgb(0.937f, 0.941f, 0.945f);
        p.border = gray(0.30f);
        p.accent = Color::rgb(0.239f, 0.682f, 0.914f);
        p.background_selected = p.accent;
        p.alternate_bg = Color::rgb(0.23f, 0.25f, 0.27f);
        break;
    }
}

static void palette_gnome(Palette &p, ColorScheme scheme) {
    p.corner_radius = 8.0f;
    Color gnomeBlue = Color::rgb(0.21f, 0.52f, 0.89f);
    p.accent = gnomeBlue;
    p.background_selected = gnomeBlue;

    switch (scheme) {
    case ColorScheme::Light:
        p.window_bg = Color::rgb(0.98f, 0.98f, 0.98f);
        p.widget_bg = gray(1.0f);
        p.input_bg = gray(1.0f);
        p.text = Color::rgb(0.18f, 0.20f, 0.21f);
        p.border = Color::rgb(0.86f, 0.84f, 0.83f);
        p.alternate_bg = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window_bg = Color::rgb(0.14f, 0.14f, 0.14f);
        p.widget_bg = Color::rgb(0.22f, 0.22f, 0.22f);
        p.input_bg = Color::rgb(0.22f, 0.22f, 0.22f);
        p.text = gray(0.95f);
        p.border = gray(0.30f);
        p.alternate_bg = Color::rgb(0.26f, 0.26f, 0.26f);
        break;
    }
}

Palette Theme::default_palette(ThemeStyle style, ColorScheme scheme) {
    Palette p;

    // Default fallback values
    p.system_font = "sans-serif";
    p.monospace_font = "monospace";
    p.font_size = 14.0f;

    if (auto *plat = detail::current_platform()) {
        auto sf = plat->system_fonts();
        if (!sf.system.empty()) {
            p.system_font = sf.system;
        }
        if (!sf.monospace.empty()) {
            p.monospace_font = sf.monospace;
        }
        if (sf.font_size > 0) {
            // Convert points to logical pixels (standard 96 DPI)
            p.font_size = std::floor(sf.font_size * (96.0f / 72.0f));
        }
        if (sf.auto_repeat_delay > 0) {
            p.auto_repeat_delay = sf.auto_repeat_delay;
        }
        if (sf.auto_repeat_interval > 0) {
            p.auto_repeat_interval = sf.auto_repeat_interval;
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
    }
    return p;
}

// ── Style-specific structural overrides ──────────────────────────────────────

static void apply_style(Theme &t, ThemeStyle style, Palette const &p) {
    bool dark = luma(p.window_bg) < 0.5f;

    switch (style) {
    case ThemeStyle::MacOS:
        t.button.padding = {4, 12, 4, 12};
        t.line_input.padding = {4, 10, 4, 10};
        t.tab_widget.tab_padding_h = 12.0f;
        t.tab_widget.tab_padding_v = 4.0f;
        break;

    case ThemeStyle::Material:
        t.button.background_hovered = dark ? p.widget_bg.lighten(0.08f) : p.widget_bg.darken(0.04f);
        t.button.background_pressed = dark ? p.widget_bg.lighten(0.15f) : p.widget_bg.darken(0.10f);
        t.button.padding = {10, 24, 10, 24};
        t.slider.handle_size = 18.0f;
        t.slider.groove_thickness = 4.0f;
        break;

    case ThemeStyle::Win11:
        t.button.background_hovered = dark ? p.widget_bg.lighten(0.06f) : p.widget_bg.darken(0.06f);
        t.button.background_pressed = dark ? p.widget_bg.lighten(0.14f) : p.widget_bg.darken(0.14f);
        t.button.padding = {6, 20, 6, 20};
        t.slider.handle_size = 20.0f;
        t.slider.groove_thickness = 4.0f;
        break;

    case ThemeStyle::Win95: {
        t.button.padding = {4, 12, 4, 12};
        t.table_view.header_border = gray(0.0f);
        t.progress_bar.fill =
            dark ? Color::rgb(0.30f, 0.50f, 0.30f) : Color::rgb(0.0f, 0.0f, 0.50f);
        t.progress_bar.chunked = true;
        t.progress_bar.beveled = false;
        t.progress_bar.border_width = 0.0f;
        t.progress_bar.bar_height = 20.0f;
        t.slider.handle_size = 12.0f;
        t.slider.groove_thickness = 4.0f;
        t.button.auto_repeat_delay = 0.4f;
        t.button.auto_repeat_interval = 0.05f;
        break;
    }

    case ThemeStyle::Plasma6: {
        t.button.background          = p.widget_bg;
        t.button.border_focused      = p.accent;
        t.button.background_hovered  = p.background_selected;
        t.button.background_pressed  = p.background_selected.darken(0.1f);
        t.button.background_selected = p.background_selected;
        t.button.corner_radius       = 5.0f;
        t.button.padding             = {6, 18, 6, 18};

        t.checkbox.background          = p.widget_bg;
        t.checkbox.border_focused      = p.accent;
        t.checkbox.corner_radius       = 5.0f;
        t.checkbox.background_selected = p.background_selected;
        t.checkbox.indicator           = gray(0.0f);

        t.radio.background          = p.widget_bg;
        t.radio.border_focused      = p.accent;
        t.radio.background_selected = p.background_selected;
        t.radio.indicator           = gray(0.0f);

        t.list_view.selected_bg     = p.accent;
        t.list_view.hovered_bg      = p.background_selected;
        t.table_view.selected_bg    = p.accent;
        t.table_view.hovered_bg     = p.background_selected;

        t.slider.handle_size = 20.0f;
        t.slider.groove_thickness = 6.0f;
        break;
    }

    case ThemeStyle::GNOME: {
        Color btn_bg = dark ? p.widget_bg.lighten(0.04f) : p.widget_bg.darken(0.03f);
        t.button.background = btn_bg;
        t.button.background_hovered = dark ? btn_bg.lighten(0.04f) : btn_bg.darken(0.04f);
        t.button.background_pressed = dark ? btn_bg.darken(0.06f) : btn_bg.darken(0.10f);
        t.button.border = dark ? p.border.lighten(0.04f) : p.border.darken(0.06f);
        t.button.padding = {8, 20, 8, 20};
        t.checkbox.corner_radius = 5.0f;
        t.checkbox.border_width = 2.0f;
        t.radio.border_width = 2.0f;
        t.line_input.corner_radius = p.corner_radius;
        t.combobox.corner_radius = p.corner_radius;
        t.slider.handle_size = 22.0f;
        t.slider.groove_thickness = 6.0f;
        break;
    }
    }
}

// ── Theme creation ───────────────────────────────────────────────────────────

Theme Theme::from_palette(std::string name, Palette const &p) {
    Theme t;
    t.name = std::move(name);
    t.system_font = p.system_font;
    t.monospace_font = p.monospace_font;

    t.window.background = p.window_bg;

    t.label.text = p.text;
    t.label.font_size = p.font_size;

    apply_base(t.button, p);
    t.button.auto_repeat_delay = p.auto_repeat_delay;
    t.button.auto_repeat_interval = p.auto_repeat_interval;

    auto mid_color = [](Color a, Color b) {
        return Color::rgb((a.r + b.r * 2) / 3, (a.g + b.g * 2) / 3, (a.b + b.b * 2) / 3);
    };
    t.button.text_disabled = mid_color(p.text, p.window_bg);

    if (!p.beveled) {
        bool dark = luma(p.widget_bg) < 0.5f;
        t.button.background_hovered = dark ? p.widget_bg.lighten(0.06f) : p.widget_bg.darken(0.06f);
        t.button.background_pressed = dark ? p.widget_bg.lighten(0.14f) : p.widget_bg.darken(0.14f);
    }

    apply_base(t.line_input, p);
    t.line_input.background = p.input_bg;
    t.line_input.background_focused = p.input_bg;
    t.line_input.border_focused = p.accent;
    t.line_input.placeholder = mid_color(p.text, p.input_bg);
    t.line_input.cursor = p.text;

    apply_base(t.text_edit, p);
    t.text_edit.background = p.input_bg;
    t.text_edit.background_focused = p.input_bg;
    t.text_edit.border_focused = p.accent;
    t.text_edit.placeholder = mid_color(p.text, p.input_bg);
    t.text_edit.cursor = p.text;

    apply_base(t.checkbox, p);
    t.checkbox.background = p.input_bg;
    t.checkbox.indicator = p.accent;

    apply_base(t.radio, p);
    t.radio.background = p.input_bg;
    t.radio.indicator = p.accent;

    apply_base(t.combobox, p);
    t.combobox.background = p.input_bg;
    bool is_dark_text = luma(p.text) < 0.5f;
    t.combobox.border_focused = p.accent;
    t.combobox.arrow = is_dark_text ? p.text.darken(0.25f) : p.text.lighten(0.25f);
    t.combobox.dropdown_bg = p.input_bg;
    t.combobox.item_hovered = p.background_selected;
    t.combobox.item_text_hovered = gray(1.0f);

    apply_base(t.tab_widget, p);
    bool is_dark = luma(p.window_bg) < 0.5f;
    t.tab_widget.tab_active_bg = p.widget_bg;
    t.tab_widget.tab_inactive_bg = is_dark ? p.window_bg.lighten(0.04f) : p.window_bg.darken(0.04f);
    t.tab_widget.tab_hover_bg = is_dark ? p.window_bg.lighten(0.08f) : p.window_bg.darken(0.02f);
    t.tab_widget.tab_active_text = p.text;
    t.tab_widget.tab_inactive_text =
        mid(p.text, is_dark ? p.window_bg.lighten(0.20f) : p.window_bg.darken(0.20f));

    apply_base(t.list_view, p);
    t.list_view.selected_bg   = p.background_selected;
    t.list_view.selected_text = gray(1.0f);
    t.list_view.hovered_bg    = is_dark ? p.widget_bg.lighten(0.06f) : p.widget_bg.darken(0.04f);
    t.list_view.alternate_bg  = p.alternate_bg;

    apply_base(t.table_view, p);
    t.table_view.selected_bg    = p.background_selected;
    t.table_view.selected_text  = gray(1.0f);
    t.table_view.hovered_bg     = is_dark ? p.widget_bg.lighten(0.06f) : p.widget_bg.darken(0.04f);
    t.table_view.alternate_bg   = p.alternate_bg;
    t.table_view.header_bg = is_dark ? p.window_bg.lighten(0.06f) : p.window_bg.darken(0.04f);
    t.table_view.header_text = p.text;
    t.table_view.header_border = p.border;
    t.table_view.grid_line = is_dark ? p.border.lighten(0.04f) : p.border.lighten(0.15f);

    apply_base(t.progress_bar, p);
    t.progress_bar.fill = p.accent;
    t.progress_bar.bar_height = 8.0f;

    apply_base(t.slider, p);
    t.slider.groove = p.border;
    t.slider.handle = p.widget_bg;
    t.slider.handle_border = p.border;
    t.slider.groove_thickness = 4.0f;
    t.slider.handle_size = 16.0f;

    if (is_dark) {
        t.tooltip.background = Color::rgb(0.25f, 0.25f, 0.22f);
        t.tooltip.border = Color::rgb(0.45f, 0.45f, 0.40f);
        t.tooltip.text = Color::rgb(0.92f, 0.92f, 0.90f);
    } else {
        t.tooltip.background = Color::rgb(1.0f, 1.0f, 0.88f);
        t.tooltip.border = Color::rgb(0.6f, 0.6f, 0.5f);
        t.tooltip.text = Color::rgb(0.1f, 0.1f, 0.1f);
    }
    t.tooltip.font_size = p.font_size - 1.0f;
    t.tooltip.corner_radius = p.corner_radius;

    return t;
}

Theme Theme::create(ThemeStyle style, Palette const &palette) {
    auto t = from_palette(style_name(style), palette);
    apply_style(t, style, palette);
    return t;
}

Theme Theme::create(ThemeStyle style, ColorScheme scheme) {
    return create(style, default_palette(style, scheme));
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
