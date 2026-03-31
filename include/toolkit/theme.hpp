// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button_state.hpp"
#include "toolkit/image_loader.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include "toolkit/widget.hpp"
#include <memory>
#include <optional>
#include <span>
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

// FIXME: remove this style
struct ButtonStyle : WidgetStyle {
    std::optional<Color> background_hovered;
    std::optional<Color> background_pressed;
    Color text_disabled;
    Margins padding = {8, 16, 8, 16};
    float auto_repeat_delay = 0.5f;
    float auto_repeat_interval = 0.4f;
};

// FIXME: remove this style
struct LineInputStyle : WidgetStyle {
    Color background_focused;
    Color border_focused;
    Color placeholder;
    Color cursor;
    Margins padding = {4, 8, 4, 8};
};

// FIXME: remove this style
struct ToggleStyle : WidgetStyle {
    Color indicator;
    float box_size = 16.0f;
    float spacing = 6.0f;
};

// FIXME: remove this style
struct ComboboxStyle : WidgetStyle {
    Color border_focused;
    Color arrow;
    Color dropdown_bg;
    Color item_hovered;
    Color item_text_hovered;
    float item_padding = 4.0f;
    Margins padding = {6, 8, 6, 8};
};

// FIXME: remove this style
struct MenuStyle : WidgetStyle {
    std::optional<Color> background_hovered;
    std::optional<Color> background_pressed;
    Color item_hovered;
    Color item_text_hovered;
    float item_padding = 4.0f;
    Margins padding = {2, 2, 2, 2};
};

// FIXME: remove this style
struct MenuBarStyle : WidgetStyle {
    std::optional<Color> background_hovered;
    std::optional<Color> background_pressed;
    Margins padding = {4, 8, 4, 8};
};

// FIXME: remove this style
struct TabWidgetStyle : WidgetStyle {
    Color tab_active_bg;
    Color tab_inactive_bg;
    Color tab_active_text;
    Color tab_inactive_text;
    Color tab_hover_bg;
    float tab_padding_h = 16.0f;
    float tab_padding_v = 6.0f;
};

// FIXME: remove this style
struct ListViewStyle : WidgetStyle {
    float item_padding = 4.0f;
    float item_padding_h = 8.0f;
};

// FIXME: remove this style
struct TableViewStyle : WidgetStyle {
    float item_padding = 4.0f;
    float item_padding_h = 8.0f;
    float header_padding_v = 6.0f;
    float default_column_width = 120.0f;
    float min_column_width = 40.0f;
};

// FIXME: remove this style
struct TreeViewStyle : WidgetStyle {
    Color selected_bg;
    Color selected_text;
    Color hovered_bg;
    Color alternate_bg;
    float item_padding = 4.0f;
    float item_padding_h = 8.0f;
    float indent = 20.0f;
};

// FIXME: remove this style
struct ProgressBarStyle : WidgetStyle {
    Color fill;
    float bar_height = 8.0f;
    bool chunked = false;
    float chunk_width = 8.0f;
    float chunk_gap = 2.0f;
};

// FIXME: remove this style
struct SliderStyle : WidgetStyle {
    Color groove;
    Color handle;
    Color handle_border;
    float groove_thickness = 4.0f;
    float handle_size = 16.0f;
};

// FIXME: remove this style
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

struct LayoutStyle {
    Margins margins = {8, 8, 8, 8};
    float spacing = 8.0f;
};

struct Palette {
    // Main background for windows
    Color window;
    // Background for input widgets/lists
    Color base;
    // Alternate background for lists
    Color alternate;
    // Normal text color
    Color text;
    // Placeholder/de-emphasized text
    Color placeholder;
    // Background for selected items
    Color highlight;
    // Text color for selected items
    Color highlighted_text;
    // Border color for widgets
    Color border;
    // Primary brand/action color
    Color accent;
    // Color for links/actions
    Color link;

    // If the palette supports shadows, a shadow, otherwise the same as border
    Color shadow;
    // If the palette supports shadows, a shadow, otherwise the same as border
    Color dark_shadow;

    // Semantic colors
    Color success;
    Color warning;
    Color error;

    float progress_bar_height = 5.0f;
    float corner_radius = 0.0f;
    float border_width = 1.0f;
    bool beveled = false;

    // Platform-specific defaults
    SystemFonts fonts;
    // FIXME: this should be platform dependent, read from desktop configuration
    float font_size = 14.0f;
    std::string system_font = "sans-serif";
    std::string monospace_font = "monospace";
    float auto_repeat_delay = 0.5f;
    float auto_repeat_interval = 0.4f;
};

enum class ColorScheme { Light, Dark };

// FIXME: now ideal.
inline constexpr int theme_style_count = 6;

enum class ThemeStyle { MacOS, Material, Win11, Win95, Plasma6, GNOME };

// FIXME: this is bad, should be in a shared include
enum class TabOrientation;

class Painter;

class Theme {
  public:
    virtual ~Theme() = default;

    // Factory methods
    static std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);
    static std::unique_ptr<Theme> create(ThemeStyle style, Palette const &palette);

    // FIXME this should be in the application, this method should be removed
    static const Theme &current();

    // FIXME: remove this function. It should be in the application.
    static void set_current(std::unique_ptr<Theme> theme);

    // FIXME: this should be in the platform code. Not here.
    static ThemeStyle detect_system_style();

    // FIXME: how about move this to the enum? or something?
    static const char *style_name(ThemeStyle style);

    // FIXME: how about move this to the enum? or something?
    static Palette default_palette(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);

    // Primitive Drawing Methods
    virtual void draw_button(Painter &painter, Rect const &rect, std::string_view text,
                             Icon const &icon, ButtonState state, bool focused, bool enabled,
                             bool flat, std::optional<Color> background = std::nullopt) const = 0;
    virtual void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text,
                               CheckState check_state, ButtonState button_state, bool focused,
                               bool enabled) const = 0;
    virtual void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text,
                                   bool checked, ButtonState button_state, bool focused,
                                   bool enabled) const = 0;
    virtual void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                                 std::string_view placeholder, int cursor_pos, int selection_start,
                                 int selection_end, bool focused, bool enabled,
                                 bool password_mode = false, float scroll_offset = 0.0f,
                                 std::optional<Color> background = std::nullopt,
                                 bool cursor_visible = true) const = 0;
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
                          bool hovered, bool enabled, TabOrientation orientation,
                          bool has_close = false, bool hovered_close = false) const = 0;
    virtual void draw_list_item(Painter &painter, Rect const &rect, std::string_view text,
                                Icon const &icon, bool selected, bool hovered,
                                bool alternate) const = 0;
    virtual void draw_list_background(Painter &painter, Rect const &rect, bool focused) const = 0;
    virtual void draw_table_background(Painter &painter, Rect const &rect, bool focused) const = 0;
    virtual void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                int depth, bool has_children, bool expanded, bool selected,
                                bool hovered, bool alternate) const = 0;
    virtual void draw_tree_background(Painter &painter, Rect const &rect, bool focused) const = 0;
    virtual void draw_combobox(Painter &painter, Rect const &rect, std::string_view text,
                               bool focused, bool open) const = 0;
    virtual void draw_combobox_item(Painter &painter, Rect const &rect, std::string_view text,
                                    bool hovered) const = 0;
    virtual void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const = 0;
    virtual void draw_toolbar(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_spinbox(Painter &painter, Rect const &rect, std::string_view text,
                              int cursor_pos, int selection_start, int selection_end, bool focused,
                              bool enabled, bool hovered_up, bool pressed_up, bool hovered_down,
                              bool pressed_down, bool cursor_visible = true) const = 0;
    virtual void draw_text_edit(Painter &painter, Rect const &rect,
                                std::span<std::string const> lines, int cursor_line, int cursor_col,
                                int selection_start_line, int selection_start_col,
                                int selection_end_line, int selection_end_col,
                                int first_visible_line, float line_height, float gutter_width,
                                float scroll_x, float scroll_y, bool focused, bool enabled,
                                std::chrono::steady_clock::time_point cursor_blink_time) const = 0;
    virtual void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const = 0;
    virtual void draw_focus_ring_for_widget(Painter &painter, Widget const *widget) const;
    virtual Size measure_label(std::string_view text, float font_size) const = 0;

    virtual Color error_color() const { return Color::rgb(1.0f, 0.85f, 0.85f); }

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
    Palette palette;
    std::string system_font;
    std::string monospace_font;

    float focus_ring_margin = 5.0f;
    float focus_ring_corner_radius = 5.0f;
    Painter::LineStyle focus_ring_line_style = Painter::LineStyle::Dashed;
    ButtonStyle button;
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
    TreeViewStyle tree_view;
    SliderStyle slider;
    TooltipStyle tooltip;
    LayoutStyle layout;
};

} // namespace toolkit
