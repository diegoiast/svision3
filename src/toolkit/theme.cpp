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


Theme const &Theme::current() {
    return mutable_current();
}

void Theme::set_current(Theme theme) {
    mutable_current() = std::move(theme);
}

static Color gray(float v) { return Color::rgb(v, v, v); }

static float luma(Color c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

static Color mid(Color a, Color b) {
    return Color::rgb((a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2);
}

static Color blend(Color a, Color b, float t) {
    return Color::rgb(a.r + (b.r - a.r) * t,
                      a.g + (b.g - a.g) * t,
                      a.b + (b.b - a.b) * t);
}

// ── Base derivation from palette ─────────────────────────────────────────────

static void apply_base(WidgetStyle &ws, Palette const &p) {
    ws.background    = p.widget_bg;
    ws.border        = p.border;
    ws.text          = p.text;
    ws.highlight     = p.highlight;
    ws.shadow        = p.shadow;
    ws.border_width  = p.border_width;
    ws.corner_radius = p.corner_radius;
    ws.font_size     = p.font_size;
    ws.beveled       = p.beveled;
}

Theme Theme::from_palette(std::string name, Palette const &p) {
    Theme t;
    t.name = std::move(name);
    t.system_font = p.system_font;
    t.monospace_font = p.monospace_font;

    t.window.background = p.window_bg;

    t.label.text      = p.text;
    t.label.font_size = p.font_size;

    apply_base(t.button, p);
    t.button.auto_repeat_delay = p.auto_repeat_delay;
    t.button.auto_repeat_interval = p.auto_repeat_interval;

    auto mid = [](Color a, Color b) {
        return Color::rgb((a.r + b.r * 2) / 3, (a.g + b.g * 2) / 3, (a.b + b.b * 2) / 3);
    };
    t.button.text_disabled       = mid(p.text, p.window_bg);

    if (!p.beveled) {
        bool dark = luma(p.widget_bg) < 0.5f;
        t.button.background_hovered = dark ? p.widget_bg.lighten(0.06f) : p.widget_bg.darken(0.06f);
        t.button.background_pressed = dark ? p.widget_bg.lighten(0.14f) : p.widget_bg.darken(0.14f);
    }

    apply_base(t.line_input, p);
    t.line_input.background         = p.input_bg;
    t.line_input.background_focused = p.input_bg;
    t.line_input.border_focused     = p.accent;
    t.line_input.placeholder        = mid(p.text, p.input_bg);
    t.line_input.cursor             = p.accent;

    t.text_edit = t.line_input;

    apply_base(t.checkbox, p);
    t.checkbox.indicator     = p.accent;
    t.checkbox.corner_radius = std::min(p.corner_radius, 3.0f);
    t.checkbox.box_size      = p.font_size + 2.0f;

    apply_base(t.radio, p);
    t.radio.indicator = p.accent;
    t.radio.box_size  = p.font_size + 2.0f;

    apply_base(t.combobox, p);
    bool is_dark_text = luma(p.text) > 0.5f;
    t.combobox.background        = p.input_bg;
    t.combobox.border_focused    = p.accent;
    t.combobox.arrow             = is_dark_text ? p.text.darken(0.25f) : p.text.lighten(0.25f);
    t.combobox.dropdown_bg       = p.input_bg;
    t.combobox.item_hovered      = p.accent;
    t.combobox.item_text_hovered = gray(1.0f);

    apply_base(t.tab_widget, p);
    bool is_dark = luma(p.window_bg) < 0.5f;
    t.tab_widget.tab_active_bg     = p.widget_bg;
    t.tab_widget.tab_inactive_bg   = is_dark ? p.window_bg.lighten(0.04f) : p.window_bg.darken(0.04f);
    t.tab_widget.tab_hover_bg      = is_dark ? p.window_bg.lighten(0.08f) : p.window_bg.darken(0.02f);
    t.tab_widget.tab_active_text   = p.text;
    t.tab_widget.tab_inactive_text = mid(p.text, is_dark ? p.window_bg.lighten(0.20f) : p.window_bg.darken(0.20f));

    apply_base(t.list_view, p);
    t.list_view.selected_bg   = p.accent;
    t.list_view.selected_text = gray(1.0f);
    t.list_view.hovered_bg    = is_dark ? p.widget_bg.lighten(0.06f) : p.widget_bg.darken(0.04f);
    t.list_view.alternate_bg  = p.alternate_bg;

    apply_base(t.table_view, p);
    t.table_view.selected_bg    = p.accent;
    t.table_view.selected_text  = gray(1.0f);
    t.table_view.hovered_bg     = is_dark ? p.widget_bg.lighten(0.06f) : p.widget_bg.darken(0.04f);
    t.table_view.alternate_bg   = p.alternate_bg;
    t.table_view.header_bg      = is_dark ? p.window_bg.lighten(0.06f) : p.window_bg.darken(0.04f);
    t.table_view.header_text    = p.text;
    t.table_view.header_border  = p.border;
    t.table_view.grid_line      = is_dark ? p.border.lighten(0.04f) : p.border.lighten(0.15f);

    apply_base(t.progress_bar, p);
    t.progress_bar.fill       = p.accent;
    t.progress_bar.bar_height = 8.0f;

    apply_base(t.slider, p);
    t.slider.groove           = p.border;
    t.slider.handle           = p.widget_bg;
    t.slider.handle_border    = p.border;
    t.slider.groove_thickness = 4.0f;
    t.slider.handle_size      = 16.0f;

    if (is_dark) {
        t.tooltip.background = Color::rgb(0.25f, 0.25f, 0.22f);
        t.tooltip.border     = Color::rgb(0.45f, 0.45f, 0.40f);
        t.tooltip.text       = Color::rgb(0.92f, 0.92f, 0.90f);
    } else {
        t.tooltip.background = Color::rgb(1.0f, 1.0f, 0.88f);
        t.tooltip.border     = Color::rgb(0.6f, 0.6f, 0.5f);
        t.tooltip.text       = Color::rgb(0.1f, 0.1f, 0.1f);
    }
    t.tooltip.font_size     = p.font_size - 1.0f;
    t.tooltip.corner_radius = p.corner_radius;

    return t;
}

// ── Style name ───────────────────────────────────────────────────────────────

const char *Theme::style_name(ThemeStyle style) {
    switch (style) {
    case ThemeStyle::MacOS:    return "macOS";
    case ThemeStyle::Material: return "Material";
    case ThemeStyle::Win11:    return "Windows 11";
    case ThemeStyle::Win95:    return "Windows 95";
    case ThemeStyle::Plasma6:  return "Plasma 6";
    case ThemeStyle::GNOME:    return "GNOME";
    }
    return "Unknown";
}

// ── Default palettes ─────────────────────────────────────────────────────────

struct SchemeColors {
    Color win, wid, txt, brd, acc, alt;
};

static void apply_scheme_colors(Palette &p, ColorScheme scheme,
                                SchemeColors const &light,
                                SchemeColors const &dark,
                                SchemeColors const &pink) {
    auto const &s = (scheme == ColorScheme::Dark) ? dark
                  : (scheme == ColorScheme::Pink) ? pink : light;
    p.window_bg    = s.win;
    p.widget_bg    = s.wid;
    p.input_bg     = s.wid;
    p.text         = s.txt;
    p.border       = s.brd;
    p.accent       = s.acc;
    p.alternate_bg = s.alt;
}

Palette Theme::default_palette(ThemeStyle style, ColorScheme scheme) {
    Palette p;
    
    // Default fallback values
    p.system_font    = "sans-serif";
    p.monospace_font = "monospace";
    p.font_size      = 14.0f;

    if (auto *plat = detail::current_platform()) {
        auto sf = plat->system_fonts();
        if (!sf.system.empty()) p.system_font = sf.system;
        if (!sf.monospace.empty()) p.monospace_font = sf.monospace;
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

    Color rose    = Color::rgb(0.89f, 0.27f, 0.50f);
    Color roseLt  = Color::rgb(0.93f, 0.40f, 0.58f);

    switch (style) {
    case ThemeStyle::MacOS:
        p.corner_radius = 6.0f;
        p.border_width  = 0.5f;
        apply_scheme_colors(p, scheme,
            {gray(0.93f),                      gray(1.0f),                       gray(0.20f), gray(0.75f), Color::rgb(0.26f, 0.52f, 0.96f), gray(0.90f)},
            {gray(0.18f),                      gray(0.24f),                      gray(0.92f), gray(0.38f), Color::rgb(0.26f, 0.52f, 0.96f), gray(0.28f)},
            {Color::rgb(0.98f, 0.92f, 0.94f), Color::rgb(1.0f,  0.97f, 0.98f), gray(0.22f), Color::rgb(0.82f, 0.70f, 0.74f), rose, Color::rgb(0.93f, 0.86f, 0.89f)});
        break;

    case ThemeStyle::Material:
        p.corner_radius = 4.0f;
        apply_scheme_colors(p, scheme,
            {gray(0.98f),                      gray(1.0f),                       gray(0.13f), gray(0.74f), Color::rgb(0.384f, 0.0f, 0.933f), gray(0.90f)},
            {Color::rgb(0.07f, 0.07f, 0.07f), Color::rgb(0.12f, 0.12f, 0.12f), gray(0.93f), gray(0.33f), Color::rgb(0.55f, 0.33f, 0.97f),  Color::rgb(0.16f, 0.16f, 0.16f)},
            {Color::rgb(0.99f, 0.93f, 0.95f), Color::rgb(1.0f,  0.97f, 0.98f), gray(0.15f), Color::rgb(0.80f, 0.68f, 0.72f), rose, Color::rgb(0.93f, 0.86f, 0.89f)});
        break;

    case ThemeStyle::Win11:
        p.corner_radius = 4.0f;
        apply_scheme_colors(p, scheme,
            {gray(0.95f),                      gray(1.0f),                       gray(0.10f), gray(0.68f), Color::rgb(0.0f, 0.47f, 0.84f), gray(0.90f)},
            {Color::rgb(0.13f, 0.13f, 0.13f), Color::rgb(0.18f, 0.18f, 0.18f), gray(0.95f), gray(0.30f), Color::rgb(0.0f, 0.47f, 0.84f), Color::rgb(0.22f, 0.22f, 0.22f)},
            {Color::rgb(0.98f, 0.93f, 0.95f), Color::rgb(1.0f,  0.97f, 0.98f), gray(0.12f), Color::rgb(0.80f, 0.68f, 0.73f), rose, Color::rgb(0.93f, 0.86f, 0.89f)});
        break;

    case ThemeStyle::Win95:
        p.beveled   = true;
        switch (scheme) {
        case ColorScheme::Light:
            p.window_bg = gray(0.75f);  p.widget_bg = gray(0.75f); p.input_bg = gray(1.0f);
            p.text = gray(0.0f);        p.border = gray(0.0f);  p.accent = gray(0.0f);
            p.highlight = gray(1.0f);   p.shadow = gray(0.50f);
            p.alternate_bg = gray(0.90f);
            break;
        case ColorScheme::Dark:
            p.window_bg = gray(0.25f);  p.widget_bg = gray(0.30f); p.input_bg = gray(0.10f);
            p.text = gray(0.90f);       p.border = gray(0.10f); p.accent = gray(0.90f);
            p.highlight = gray(0.45f);  p.shadow = gray(0.12f);
            p.alternate_bg = gray(0.35f);
            break;
        case ColorScheme::Pink:
            p.window_bg = Color::rgb(0.78f, 0.65f, 0.70f);
            p.widget_bg = Color::rgb(0.78f, 0.65f, 0.70f);
            p.input_bg  = gray(1.0f);
            p.text = gray(0.0f);        p.border = gray(0.0f);  p.accent = gray(0.0f);
            p.highlight = Color::rgb(1.0f, 0.88f, 0.92f);
            p.shadow    = Color::rgb(0.50f, 0.35f, 0.40f);
            p.alternate_bg = Color::rgb(0.73f, 0.60f, 0.65f);
            break;
        }
        break;

    case ThemeStyle::Plasma6:
        p.corner_radius = 3.0f;
        apply_scheme_colors(p, scheme,
            {Color::rgb(0.937f, 0.941f, 0.945f), gray(1.0f),                            Color::rgb(0.137f, 0.149f, 0.161f), Color::rgb(0.737f, 0.753f, 0.773f), Color::rgb(0.239f, 0.682f, 0.914f), gray(0.90f)},
            {Color::rgb(0.137f, 0.149f, 0.161f), Color::rgb(0.192f, 0.212f, 0.231f),   Color::rgb(0.937f, 0.941f, 0.945f), gray(0.30f),                         Color::rgb(0.239f, 0.682f, 0.914f), Color::rgb(0.23f, 0.25f, 0.27f)},
            {Color::rgb(0.97f, 0.92f, 0.94f),    Color::rgb(1.0f,  0.97f, 0.98f),      Color::rgb(0.16f, 0.15f, 0.16f),    Color::rgb(0.80f, 0.70f, 0.74f),    roseLt, Color::rgb(0.93f, 0.86f, 0.89f)});
        break;

    case ThemeStyle::GNOME:
        p.corner_radius = 8.0f;
        apply_scheme_colors(p, scheme,
            {Color::rgb(0.98f, 0.98f, 0.98f), gray(1.0f),                       Color::rgb(0.18f, 0.20f, 0.21f), Color::rgb(0.86f, 0.84f, 0.83f), Color::rgb(0.21f, 0.52f, 0.89f), gray(0.90f)},
            {Color::rgb(0.14f, 0.14f, 0.14f), Color::rgb(0.22f, 0.22f, 0.22f), gray(0.95f),                      gray(0.30f),                      Color::rgb(0.21f, 0.52f, 0.89f), Color::rgb(0.26f, 0.26f, 0.26f)},
            {Color::rgb(0.99f, 0.93f, 0.95f), Color::rgb(1.0f,  0.97f, 0.98f), Color::rgb(0.20f, 0.18f, 0.19f), Color::rgb(0.84f, 0.76f, 0.79f), rose, Color::rgb(0.93f, 0.86f, 0.89f)});
        break;
    }
    return p;
}

// ── Style-specific structural overrides ──────────────────────────────────────

static void apply_style(Theme &t, ThemeStyle style, Palette const &p) {
    bool dark = luma(p.widget_bg) < 0.5f;

    switch (style) {
    case ThemeStyle::MacOS:
        t.button.padding = {6, 16, 6, 16};
        t.slider.handle_size = 20.0f;
        t.slider.groove_thickness = 4.0f;
        break;

    case ThemeStyle::Material:
        t.button.background         = p.accent;
        t.button.background_hovered = p.accent.lighten(0.1f);
        t.button.background_pressed = p.accent.darken(0.1f);
        t.button.border             = Color::rgba(0, 0, 0, 0);
        t.button.text               = gray(1.0f);
        t.button.border_width       = 0.0f;
        t.button.padding            = {10, 24, 10, 24};
        t.checkbox.border_width     = 2.0f;
        t.radio.border_width        = 2.0f;
        t.line_input.padding        = {6, 12, 6, 12};
        t.slider.handle_size        = 14.0f;
        t.slider.groove_thickness   = 2.0f;
        t.slider.handle             = p.accent;
        t.slider.handle_border      = Color::rgba(0, 0, 0, 0);
        break;

    case ThemeStyle::Win11: {
        Color btn_bg = dark ? p.widget_bg.lighten(0.02f) : p.widget_bg.darken(0.02f);
        t.button.background         = btn_bg;
        t.button.background_hovered = blend(btn_bg, p.accent, dark ? 0.12f : 0.06f);
        t.button.background_pressed = blend(btn_bg, p.accent, dark ? 0.06f : 0.15f);
        t.button.border             = dark ? p.border.lighten(0.04f) : p.border.lighten(0.08f);
        t.button.padding            = {6, 20, 6, 20};
        t.slider.handle_size        = 18.0f;
        t.slider.groove_thickness   = 4.0f;
        t.slider.handle             = p.accent;
        break;
    }

    case ThemeStyle::Win95: {
        Color navy = dark ? Color::rgb(0.20f, 0.20f, 0.60f)
                          : Color::rgb(0.0f, 0.0f, 0.50f);
        t.button.background  = p.widget_bg;
        t.button.padding     = {4, 12, 4, 12};
        t.line_input.padding = {3, 4, 3, 4};
        t.combobox.item_hovered    = navy;
        t.list_view.selected_bg    = navy;
        t.list_view.selected_text  = gray(1.0f);
        t.progress_bar.bar_height  = 20.0f;
        t.progress_bar.chunked     = true;
        t.progress_bar.chunk_width = 10.0f;
        t.progress_bar.chunk_gap   = 2.0f;
        t.progress_bar.fill        = dark
            ? Color::rgb(0.30f, 0.50f, 0.30f)
            : Color::rgb(0.0f, 0.0f, 0.50f);
        t.slider.handle_size = 12.0f;
        t.slider.groove_thickness = 4.0f;
        t.button.auto_repeat_delay = 0.4f;
        t.button.auto_repeat_interval = 0.05f;
        break;
    }

    case ThemeStyle::Plasma6: {
        Color btn_bg = dark ? p.widget_bg.lighten(0.03f) : p.widget_bg.darken(0.04f);
        t.button.background         = btn_bg;
        t.button.background_hovered = p.accent;
        t.button.background_pressed = p.accent.darken(0.06f);
        t.button.padding            = {6, 18, 6, 18};
        t.slider.handle_size        = 20.0f;
        t.slider.groove_thickness   = 6.0f;
        break;
    }

    case ThemeStyle::GNOME: {
        Color btn_bg = dark ? p.widget_bg.lighten(0.04f) : p.widget_bg.darken(0.03f);
        t.button.background         = btn_bg;
        t.button.background_hovered = dark ? btn_bg.lighten(0.04f) : btn_bg.darken(0.04f);
        t.button.background_pressed = dark ? btn_bg.darken(0.06f) : btn_bg.darken(0.10f);
        t.button.border             = dark ? p.border.lighten(0.04f) : p.border.darken(0.06f);
        t.button.padding            = {8, 20, 8, 20};
        t.checkbox.corner_radius    = 5.0f;
        t.checkbox.border_width     = 2.0f;
        t.radio.border_width        = 2.0f;
        t.line_input.corner_radius  = p.corner_radius;
        t.combobox.corner_radius    = p.corner_radius;
        t.slider.handle_size        = 22.0f;
        t.slider.groove_thickness   = 6.0f;
        break;
    }
    }
}

// ── Theme creation ───────────────────────────────────────────────────────────

Theme Theme::create(ThemeStyle style, Palette const &palette) {
    auto t = from_palette(style_name(style), palette);
    apply_style(t, style, palette);
    return t;
}

Theme Theme::create(ThemeStyle style, ColorScheme scheme) {
    return create(style, default_palette(style, scheme));
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
            auto it = std::search(s.begin(), s.end(), sub.begin(), sub.end(),
                                  [](char a, char b) {
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
