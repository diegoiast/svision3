// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace toolkit {

// FIXME: this should move to the color struct
static Color gray(float v) { return Color::rgb(v, v, v); }

// FIXME: this should move to the color struct
static float luma(Color c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

// FIXME: this should move to the color struct, with a multiplier (not only 0.5)
static Color mid(Color a, Color b) {
    return Color::rgb((a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2);
}

// ── BaseTheme Implementation ─────────────────────────────────────────────────

class BaseTheme : public Theme {
  public:
    explicit BaseTheme(Palette p) : palette_(std::move(p)) {
        // Initialize backward compatibility members
        name = "Base";
        style = ThemeStyle::Material;
        system_font = palette_.fonts.system;
        monospace_font = palette_.fonts.monospace;

        window.background = palette_.window;

        label.text = palette_.text;
        label.font_size = palette_.fonts.font_size;

        auto apply_base = [](WidgetStyle &ws, Palette const &p) {
            ws.background = p.window;
            ws.border = p.border;
            ws.border_focused = p.border;
            ws.text = p.text;
            ws.background_selected = p.highlight;
            ws.highlight = p.highlight;
            ws.shadow = p.shadow;
            ws.font_size = p.fonts.font_size;
        };

        apply_base(button, palette_);
        button.auto_repeat_delay = palette_.fonts.auto_repeat_delay;
        button.auto_repeat_interval = palette_.fonts.auto_repeat_interval;
        button.text_disabled = mid(palette_.text, palette_.window);

        apply_base(line_input, palette_);
        line_input.background = palette_.base;
        line_input.background_focused = palette_.base;
        line_input.border_focused = palette_.accent;
        line_input.placeholder = mid(palette_.text, palette_.base);
        line_input.cursor = palette_.text;

        apply_base(text_edit, palette_);
        text_edit.background = palette_.base;
        text_edit.background_focused = palette_.base;
        text_edit.border_focused = palette_.accent;
        text_edit.placeholder = mid(palette_.text, palette_.base);
        text_edit.cursor = palette_.text;

        apply_base(checkbox, palette_);
        checkbox.background = palette_.base;
        checkbox.indicator = palette_.accent;

        apply_base(radio, palette_);
        radio.background = palette_.base;
        radio.indicator = palette_.accent;

        apply_base(combobox, palette_);
        combobox.background = palette_.base;
        combobox.border_focused = palette_.accent;
        combobox.arrow =
            luma(palette_.text) < 0.5f ? palette_.text.darken(0.25f) : palette_.text.lighten(0.25f);
        combobox.dropdown_bg = palette_.base;
        combobox.item_hovered = palette_.highlight;
        combobox.item_text_hovered = gray(1.0f);

        apply_base(menu, palette_);
        menu.background = palette_.base;
        menu.background_hovered = palette_.highlight;
        menu.item_hovered = palette_.highlight;
        menu.item_text_hovered = gray(1.0f);

        apply_base(menubar, palette_);
        menubar.background = palette_.window;
        menubar.background_hovered = palette_.highlight;

        apply_base(tab_widget, palette_);
        bool is_dark = luma(palette_.window) < 0.5f;
        tab_widget.tab_active_bg = palette_.window;
        tab_widget.tab_inactive_bg =
            is_dark ? palette_.window.lighten(0.04f) : palette_.window.darken(0.04f);
        tab_widget.tab_hover_bg =
            is_dark ? palette_.window.lighten(0.08f) : palette_.window.darken(0.02f);
        tab_widget.tab_active_text = palette_.text;
        tab_widget.tab_inactive_text = mid(palette_.text, is_dark ? palette_.window.lighten(0.20f)
                                                                  : palette_.window.darken(0.20f));

        apply_base(list_view, palette_);
        list_view.selected_bg = palette_.highlight;
        list_view.selected_text = gray(1.0f);
        list_view.hovered_bg =
            is_dark ? palette_.window.lighten(0.06f) : palette_.window.darken(0.04f);
        list_view.alternate_bg = palette_.alternate;

        apply_base(table_view, palette_);
        table_view.selected_bg = palette_.highlight;
        table_view.selected_text = gray(1.0f);
        table_view.hovered_bg =
            is_dark ? palette_.window.lighten(0.06f) : palette_.window.darken(0.04f);
        table_view.alternate_bg = palette_.alternate;
        table_view.header_bg =
            is_dark ? palette_.window.lighten(0.06f) : palette_.window.darken(0.04f);
        table_view.header_text = palette_.text;
        table_view.header_border = palette_.border;
        table_view.grid_line =
            is_dark ? palette_.border.lighten(0.04f) : palette_.border.lighten(0.15f);

        apply_base(progress_bar, palette_);
        progress_bar.fill = palette_.accent;

        apply_base(slider, palette_);
        slider.groove = palette_.border;
        slider.handle = palette_.window;
        slider.handle_border = palette_.border;

        if (is_dark) {
            tooltip.background = Color::rgb(0.25f, 0.25f, 0.22f);
            tooltip.border = Color::rgb(0.45f, 0.45f, 0.40f);
            tooltip.text = Color::rgb(0.92f, 0.92f, 0.90f);
        } else {
            tooltip.background = Color::rgb(1.0f, 1.0f, 0.88f);
            tooltip.border = Color::rgb(0.6f, 0.6f, 0.5f);
            tooltip.text = Color::rgb(0.1f, 0.1f, 0.1f);
        }
        tooltip.font_size = palette_.fonts.font_size - 1.0f;
    }

    void draw_button(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                     bool hovered, bool pressed, bool focused, bool enabled,
                     bool flat) const override {
        // Fallback to widget logic for now during migration phase 3
    }

    void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text, CheckState state,
                       bool hovered, bool pressed, bool focused, bool enabled) const override {}

    void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text, bool checked,
                           bool hovered, bool pressed, bool focused, bool enabled) const override {}

    void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                         std::string_view placeholder, int cursor_pos, int selection_start,
                         int selection_end, bool focused, bool enabled) const override {}

    void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title, bool hovered,
                           bool active, bool show_mnemonics, int mnemonic_index) const override {
        auto const &style = menubar;
        auto padding = style.padding;
        auto fm = painter.font_metrics(style.font_size);

        if (hovered || active) {
            Color bg = style.background_hovered.value_or(style.background.darken(0.1f));
            painter.fill_rect(rect, bg);
        }

        auto baseline = (rect.height - fm.height) / 2.0f + fm.ascent;
        Color text_c = style.text;
        painter.draw_text(title, {rect.x + padding.left, baseline}, text_c, style.font_size);

        if (show_mnemonics && mnemonic_index >= 0) {
            auto before = title.substr(0, mnemonic_index);
            auto ch = std::string(1, title[mnemonic_index]);
            auto before_w =
                before.empty() ? 0.0f : painter.text_size(before, style.font_size).width;
            auto ch_w = painter.text_size(ch, style.font_size).width;
            auto ul_y = baseline + fm.descent * 0.4f;

            painter.draw_line({rect.x + padding.left + before_w, ul_y},
                              {rect.x + padding.left + before_w + ch_w, ul_y}, text_c, 1.0f);
        }
    }

    void draw_menubar_background(Painter &painter, Rect const &rect) const override {
        painter.fill_rect(rect, menubar.background);
    }

    void draw_menu_background(Painter &painter, Rect const &rect) const override {
        painter.fill_rect(rect, menu.background);
        painter.draw_rect(rect, menu.border, menu.border_width);
    }

    void draw_menu_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        std::string_view shortcut, bool hovered, bool enabled, bool checkable,
                        bool checked) const override {}

    void draw_menu_separator(Painter &painter, Rect const &rect) const override {}

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           bool enabled) const override {}

    void draw_slider(Painter &painter, Rect const &rect, float value, bool hovered, bool pressed,
                     bool focused, bool enabled) const override {}

    void draw_tab_bar_background(Painter &painter, Rect const &rect) const override {}

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled) const override {}

    void draw_list_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        bool selected, bool hovered, bool alternate) const override {}

    void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const override {}

    Size measure_button(std::string_view text, Icon const &icon) const override { return {0, 0}; }

    Size measure_menubar_item(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, menubar.font_size).width;
        return {text_w + menubar.padding.left + menubar.padding.right, 0};
    }

    Size measure_menu_item(std::string_view text, Icon const &icon,
                           std::string_view shortcut) const override {
        return {0, 0};
    }

    float menu_separator_height() const override { return 8.0f; }
    Size measure_tab(std::string_view text) const override { return {0, 0}; }
    float list_item_height() const override { return 24.0f; }
    Size measure_tooltip(std::string_view text) const override { return {0, 0}; }

    Margins button_padding() const override { return button.padding; }
    Margins line_input_padding() const override { return line_input.padding; }

  protected:
    Palette palette_;
};

// ── Specific Themes ──────────────────────────────────────────────────────────

class Win11Theme : public BaseTheme {
  public:
    using BaseTheme::BaseTheme;
    void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title, bool hovered,
                           bool active, bool show_mnemonics, int mnemonic_index) const override {
        auto const &style = menubar;
        auto padding = style.padding;
        auto fm = painter.font_metrics(style.font_size);

        if (hovered || active) {
            Color bg = style.background_hovered.value_or(style.background.darken(0.1f));
            auto hover_rect = rect.inset(2.0f);
            painter.fill_rounded_rect(hover_rect, bg, style.corner_radius);
        }

        auto baseline = (rect.height - fm.height) / 2.0f + fm.ascent;
        Color text_c = style.text;
        if (hovered || active) {
            if (luma(style.background_hovered.value_or(Color::rgb(0, 0, 0))) < 0.5f) {
                text_c = Color::rgb(1, 1, 1);
            }
        }
        painter.draw_text(title, {rect.x + padding.left, baseline}, text_c, style.font_size);
    }
};

class Win95Theme : public BaseTheme {
  public:
    explicit Win95Theme(Palette p) : BaseTheme(std::move(p)) {
        name = "Windows 95";
        progress_bar.chunked = true;
        progress_bar.bar_height = 20.0f;
    }
};

// ── Theme Management ─────────────────────────────────────────────────────────

static std::unique_ptr<Theme> &mutable_current_ptr() {
    static std::unique_ptr<Theme> instance;
    if (!instance) {
        instance = Theme::create(Theme::detect_system_style());
    }
    return instance;
}

Theme const &Theme::current() { return *mutable_current_ptr(); }

void Theme::set_current(std::unique_ptr<Theme> theme) { mutable_current_ptr() = std::move(theme); }

// ── Palette Initialization ───────────────────────────────────────────────────

static void palette_macos(Palette &p, ColorScheme scheme) {
    p.border_width = 0.5f;
    Color macBlue = Color::rgb(0.0f, 0.48f, 1.0f);
    p.accent = macBlue;
    p.highlight = macBlue;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = gray(0.93f);
        p.base = gray(1.0f);
        p.text = gray(0.20f);
        p.border = gray(0.75f);
        p.alternate = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window = gray(0.18f);
        p.base = gray(0.24f);
        p.text = gray(0.92f);
        p.border = gray(0.38f);
        p.alternate = gray(0.28f);
        break;
    }
}

static void palette_win11(Palette &p, ColorScheme scheme) {
    Color winBlue = Color::rgb(0.0f, 0.47f, 0.84f);
    p.accent = winBlue;
    p.highlight = winBlue;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = gray(0.95f);
        p.base = gray(1.0f);
        p.text = gray(0.10f);
        p.border = gray(0.68f);
        p.alternate = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window = Color::rgb(0.13f, 0.13f, 0.13f);
        p.base = Color::rgb(0.18f, 0.18f, 0.18f);
        p.text = gray(0.95f);
        p.border = gray(0.30f);
        p.alternate = Color::rgb(0.22f, 0.22f, 0.22f);
        break;
    }
}

static void palette_win95(Palette &p, ColorScheme scheme) {
    p.beveled = true;
    switch (scheme) {
    case ColorScheme::Light:
        p.window = gray(0.75f);
        p.base = gray(1.0f);
        p.text = gray(0.0f);
        p.border = gray(0.0f);
        p.accent = Color::rgb(0.0f, 0.0f, 0.5f);
        p.highlight = Color::rgb(0.0f, 0.0f, 0.5f);
        p.shadow = gray(0.50f);
        p.alternate = gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window = gray(0.25f);
        p.base = gray(0.10f);
        p.text = gray(0.90f);
        p.border = gray(0.10f);
        p.accent = Color::rgb(0.0f, 0.0f, 0.8f);
        p.highlight = Color::rgb(0.0f, 0.0f, 0.8f);
        p.shadow = gray(0.12f);
        p.alternate = gray(0.35f);
        break;
    }
}

Palette Theme::default_palette(ThemeStyle style, ColorScheme scheme) {
    Palette p;
    p.fonts.system = "sans-serif";
    p.fonts.monospace = "monospace";
    p.fonts.font_size = 14.0f;
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
        if (sf.font_size > 0) {
            p.fonts.font_size = std::floor(sf.font_size * (96.0f / 72.0f));
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
    case ThemeStyle::Win11:
        palette_win11(p, scheme);
        break;
    case ThemeStyle::Win95:
        palette_win95(p, scheme);
        break;
    default:
        palette_win11(p, scheme);
        break;
    }

    // Sync backward compatibility fields
    p.window_bg = p.window;
    p.widget_bg = p.window;
    p.input_bg = p.base;
    p.font_size = p.fonts.font_size;
    p.system_font = p.fonts.system;
    p.monospace_font = p.fonts.monospace;
    p.auto_repeat_delay = p.fonts.auto_repeat_delay;
    p.auto_repeat_interval = p.fonts.auto_repeat_interval;

    return p;
}

std::unique_ptr<Theme> Theme::create(ThemeStyle style, ColorScheme scheme) {
    return create(style, default_palette(style, scheme));
}

std::unique_ptr<Theme> Theme::create(ThemeStyle style, Palette const &palette) {
    std::unique_ptr<Theme> t;
    switch (style) {
    case ThemeStyle::Win11:
        t = std::make_unique<Win11Theme>(palette);
        break;
    case ThemeStyle::Win95:
        t = std::make_unique<Win95Theme>(palette);
        break;
    default:
        t = std::make_unique<BaseTheme>(palette);
        break;
    }
    t->name = style_name(style);
    t->style = style;
    return t;
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
