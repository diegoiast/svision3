// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image_loader.hpp"
#include "toolkit/types.hpp"
#include <memory>
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

struct MenuStyle : WidgetStyle {
    std::optional<Color> background_hovered;
    std::optional<Color> background_pressed;
    Color item_hovered;
    Color item_text_hovered;
    float item_padding = 4.0f;
    Margins padding = {2, 2, 2, 2};
};

struct MenuBarStyle : WidgetStyle {
    std::optional<Color> background_hovered;
    std::optional<Color> background_pressed;
    Margins padding = {4, 8, 4, 8};
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
    Color window;           // Main background for windows
    Color base;             // Background for input widgets/lists
    Color alternate;        // Alternate background for lists
    Color text;             // Normal text color
    Color placeholder;      // Placeholder/de-emphasized text
    Color highlight;        // Background for selected items
    Color highlighted_text; // Text color for selected items
    Color border;           // Border color for widgets
    Color accent;           // Primary brand/action color
    Color link;             // Color for links/actions

    // Semantic colors
    Color success;
    Color warning;
    Color error;

    // Platform-specific defaults
    SystemFonts fonts;

    // Backward compatibility fields
    Color window_bg;
    Color widget_bg;
    Color input_bg;
    float font_size = 14.0f;
    std::string system_font = "sans-serif";
    std::string monospace_font = "monospace";
    float corner_radius = 0.0f;
    float border_width = 1.0f;
    bool beveled = false;
    Color shadow;
    float auto_repeat_delay = 0.5f;
    float auto_repeat_interval = 0.4f;
};

enum class ColorScheme { Light, Dark };

enum class ThemeStyle { MacOS, Material, Win11, Win95, Plasma6, GNOME };

inline constexpr int theme_style_count = 6;

class Painter;

class Theme {
  public:
    virtual ~Theme() = default;

    // Factory methods
    static std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);
    static std::unique_ptr<Theme> create(ThemeStyle style, Palette const &palette);
    static const Theme &current();
    static void set_current(std::unique_ptr<Theme> theme);
    static ThemeStyle detect_system_style();
    static const char *style_name(ThemeStyle style);
    static Palette default_palette(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);

    // Primitive Drawing Methods
    virtual void draw_button(Painter &painter, Rect const &rect, std::string_view text,
                             Icon const &icon, bool hovered, bool pressed, bool focused,
                             bool enabled, bool flat) const = 0;
    virtual void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text,
                               CheckState state, bool hovered, bool pressed, bool focused,
                               bool enabled) const = 0;
    virtual void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text,
                                   bool checked, bool hovered, bool pressed, bool focused,
                                   bool enabled) const = 0;
    virtual void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                                 std::string_view placeholder, int cursor_pos, int selection_start,
                                 int selection_end, bool focused, bool enabled) const = 0;
    virtual void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title,
                                   bool hovered, bool active, bool show_mnemonics,
                                   int mnemonic_index) const = 0;
    virtual void draw_menubar_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_menu_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_menu_item(Painter &painter, Rect const &rect, std::string_view text,
                                Icon const &icon, std::string_view shortcut, bool hovered,
                                bool enabled, bool checkable, bool checked) const = 0;
    virtual void draw_menu_separator(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                                   bool enabled) const = 0;
    virtual void draw_slider(Painter &painter, Rect const &rect, float value, bool horizontal,
                             bool hovered, bool pressed, bool focused, bool enabled) const = 0;
    virtual void draw_tab_bar_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                          bool hovered, bool enabled) const = 0;
    virtual void draw_list_item(Painter &painter, Rect const &rect, std::string_view text,
                                Icon const &icon, bool selected, bool hovered,
                                bool alternate) const = 0;
    virtual void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const = 0;
    virtual void draw_toolbar(Painter &painter, Rect const &rect) const = 0;
    virtual Size measure_label(std::string_view text, float font_size) const = 0;

    // Metrics and Styles
    virtual Size measure_button(std::string_view text, Icon const &icon) const = 0;
    virtual Size measure_checkbox(std::string_view text) const = 0;
    virtual Size measure_radio_button(std::string_view text) const = 0;
    virtual Size measure_menubar_item(std::string_view text) const = 0;
    virtual Size measure_menu_item(std::string_view text, Icon const &icon,
                                   std::string_view shortcut) const = 0;
    virtual float menu_separator_height() const = 0;
    virtual Size measure_tab(std::string_view text) const = 0;
    virtual float list_item_height() const = 0;
    virtual Size measure_tooltip(std::string_view text) const = 0;

    virtual Margins button_padding() const = 0;
    virtual Margins line_input_padding() const = 0;

    // Members for backward compatibility during migration
    std::string name;
    ThemeStyle style;
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
    MenuStyle menu;
    MenuBarStyle menubar;
    TabWidgetStyle tab_widget;
    ListViewStyle list_view;
    TableViewStyle table_view;
    ProgressBarStyle progress_bar;
    SliderStyle slider;
    TooltipStyle tooltip;
};

} // namespace toolkit
