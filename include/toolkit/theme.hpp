// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"
#include <optional>
#include <string>

namespace toolkit {

struct WidgetStyle {
    Color background;
    Color border;
    Color border_focused;
    Color text;
    Color background_selected = Color::rgba(0, 0, 0, 0);
    Color highlight = Color::rgba(0, 0, 0, 0);
    Color shadow = Color::rgba(0, 0, 0, 0);
    float border_width = 1.0f;
    float corner_radius = 0.0f;
    float font_size = 14.0f;
    bool beveled = false;
};

struct ButtonStyle : WidgetStyle {
    std::optional<Color> background_hovered;
    std::optional<Color> background_pressed;
    Color text_disabled;
    Margins padding = {8, 16, 8, 16};
    float auto_repeat_delay = 0.5f;
    float auto_repeat_interval = 0.4f;
};

struct LabelStyle {
    Color text;
    float font_size = 14.0f;
};

struct LineInputStyle : WidgetStyle {
    Color background_focused;
    Color border_focused;
    Color placeholder;
    Color cursor;
    Margins padding = {4, 8, 4, 8};
};

struct ToggleStyle : WidgetStyle {
    Color indicator;
    float box_size = 16.0f;
    float spacing = 6.0f;
};

struct ComboboxStyle : WidgetStyle {
    Color border_focused;
    Color arrow;
    Color dropdown_bg;
    Color item_hovered;
    Color item_text_hovered;
    float item_padding = 4.0f;
    Margins padding = {6, 8, 6, 8};
};

struct TabWidgetStyle : WidgetStyle {
    Color tab_active_bg;
    Color tab_inactive_bg;
    Color tab_active_text;
    Color tab_inactive_text;
    Color tab_hover_bg;
    float tab_padding_h = 16.0f;
    float tab_padding_v = 6.0f;
};

struct ListViewStyle : WidgetStyle {
    Color selected_bg;
    Color selected_text;
    Color hovered_bg;
    Color alternate_bg;
    float item_padding = 4.0f;
    float item_padding_h = 8.0f;
};

struct TableViewStyle : WidgetStyle {
    Color selected_bg;
    Color selected_text;
    Color hovered_bg;
    Color alternate_bg;
    Color header_bg;
    Color header_text;
    Color header_border;
    Color grid_line;
    float item_padding = 4.0f;
    float item_padding_h = 8.0f;
    float header_padding_v = 6.0f;
    float default_column_width = 120.0f;
    float min_column_width = 40.0f;
};

struct ProgressBarStyle : WidgetStyle {
    Color fill;
    float bar_height = 8.0f;
    bool chunked = false;
    float chunk_width = 8.0f;
    float chunk_gap = 2.0f;
};

struct SliderStyle : WidgetStyle {
    Color groove;
    Color handle;
    Color handle_border;
    float groove_thickness = 4.0f;
    float handle_size = 16.0f;
};

struct TooltipStyle {
    Color background = Color::rgb(1.0f, 1.0f, 0.88f);
    Color border = Color::rgb(0.6f, 0.6f, 0.5f);
    Color text = Color::rgb(0.1f, 0.1f, 0.1f);
    float font_size = 12.0f;
    float padding = 5.0f;
    float corner_radius = 3.0f;
    float border_width = 1.0f;
    float delay_sec = 0.6f;
};

struct WindowStyle {
    Color background;
};

struct Palette {
    Color window_bg;
    Color widget_bg = Color::rgb(1, 1, 1);
    Color input_bg = Color::rgb(1, 1, 1);
    Color text;
    Color border;
    Color accent;
    Color background_selected = Color::rgba(0, 0, 0, 0);
    Color alternate_bg = Color::rgb(0.94f, 0.94f, 0.94f);
    float font_size = 14.0f;
    std::string system_font = "sans-serif";
    std::string monospace_font = "monospace";
    float corner_radius = 0.0f;
    float border_width = 1.0f;
    bool beveled = false;
    Color highlight = Color::rgba(0, 0, 0, 0);
    Color shadow = Color::rgba(0, 0, 0, 0);
    float auto_repeat_delay = 0.5f;
    float auto_repeat_interval = 0.4f;
};

enum class ColorScheme { Light, Dark };

enum class ThemeStyle { MacOS, Material, Win11, Win95, Plasma6, GNOME };

inline constexpr int theme_style_count = 6;

struct Theme {
    std::string name;
    std::string system_font;
    std::string monospace_font;
    WindowStyle window;
    ButtonStyle button;
    LabelStyle label;
    LineInputStyle line_input;
    LineInputStyle text_edit;
    ToggleStyle checkbox;
    ToggleStyle radio;
    ComboboxStyle combobox;
    TabWidgetStyle tab_widget;
    ListViewStyle list_view;
    TableViewStyle table_view;
    ProgressBarStyle progress_bar;
    SliderStyle slider;
    TooltipStyle tooltip;

    static Theme from_palette(std::string name, Palette const &p);

    static const char *style_name(ThemeStyle style);
    static Palette default_palette(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);
    static Theme create(ThemeStyle style, Palette const &palette);
    static Theme create(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);

    static Theme const &current();
    static void set_current(Theme theme);
    static ThemeStyle detect_system_style();
};

} // namespace toolkit
