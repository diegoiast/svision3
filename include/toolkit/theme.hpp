// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button_state.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include "toolkit/widget.hpp"
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace toolkit {

struct Style {
    struct {
        bool bottom_shadow = false;
        float menu_indicator_width = 10.0f;
        Margins padding = {6, 16, 6, 16};
    } button;

    struct {
        Margins padding = {6, 8, 6, 8};
    } lineInput;

    struct ToggleStyle {
        float box_size = 16.0f;
        float spacing = 6.0f;
        bool accent_fill = false;
    } toggle;

    struct {
        float item_padding = 4.0f;
        Margins padding = {6, 8, 6, 8};
    } combo;

    struct {
        float item_padding = 4.0f;
        Margins padding = {2, 2, 2, 2};
    } menu;

    struct {
        Margins padding = {6, 8, 6, 8};
    } menuBar;

    struct TabWidgetStyle {
        float tab_padding_h = 16.0f;
        float tab_padding_v = 6.0f;
        float tab_radius = 0.0f;
        float tab_padding = 0.0f;
        bool tab_fully_rounded = false;
        std::optional<float> indicator_weight;
    } tabWidget;

    struct IconGridStyle {
        float spacing = 8.0f;
        Margins padding = {8, 8, 8, 8};
    } iconGrid;

    struct ListViewStyle {
        float item_padding = 4.0f;
        float item_padding_h = 8.0f;
    } listView;

    struct TableViewStyle {
        float item_padding = 4.0f;
        float item_padding_h = 8.0f;
        float header_padding_v = 6.0f;
        float default_column_width = 120.0f;
        float min_column_width = 40.0f;
    } tableView;

    struct TreeViewStyle {
        float item_padding = 4.0f;
        float item_padding_h = 8.0f;
        float indent = 20.0f;
    } treeView;

    struct {
        float groove_thickness = 4.0f;
        float handle_size = 16.0f;
    } slider;

    struct TooltipStyle {
        float delay_sec = 0.6f;
        float padding = 4.0f;
    } tooltip;

    struct {
        float thickness = 16.0f;
        float button_size = 16.0f;
        Margins padding = {0, 0, 0, 0};
        bool show_buttons = true;
        bool show_frame = true;
    } scrollbar;

    struct LayoutStyle {
        Margins margins = {4, 4, 4, 4};
        float spacing = 4.0f;
    } layout;

    struct {
#ifdef _WIN32
        unsigned long size = 0;
#else
        unsigned long size = 48;
#endif
        float opacity = 0.6f;
    } shadow;

    struct {
        float height = 5.0f;
    } progressBar;

    struct {
        float margin = 5.0f;
        float corner_radius = 5.0f;
        Painter::LineStyle line_style = Painter::LineStyle::Dashed;
    } ringFocus;

    float corner_radius = 0.0f;
    float border_width = 1.0f;
    bool beveled = false;
    bool chrome_lines = true;
    bool inline_scrollbars = true;
    bool bottom_shadow = false;

    Margins window_decoration = {0, 0, 0, 0};

    // Inset in pixels from the widget edge to the inner content area, accounting for border style
    float frame_inset() const { return beveled ? 1.0f : std::ceil(border_width); }
};

struct Palette {
    // Main background for windows
    Color window;
    std::optional<Color> window_inactive;
    // Background for input widgets/lists
    Color base;
    // Alternate background for lists
    Color alternate;
    // Normal text color
    Color text;
    // Disabled text color
    Color text_disabled;
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
    // Background color of tooltip windows
    Color tooltip;

    Color tab_select_background;
    Color tab_background;

    // If the palette supports shadows, a shadow, otherwise the same as border
    Color light;
    Color shadow;
    Color dark_shadow;

    // Backgrond color used by buttons, may be ommited.
    std::optional<Color> background_pressed;
    // Backgrond color used by buttons, may be ommited.
    std::optional<Color> background_hovered;

    // Semantic colors
    Color success;
    Color warning;
    Color error;

    void set_accent(Color color) { accent = color; }

    // FIXME: this should be platform dependent, read from desktop configuration
    SystemFonts fonts;
    float auto_repeat_delay = 0.5f;
    float auto_repeat_interval = 0.4f;
};

enum class ColorScheme { Light, Dark };

// FIXME: not ideal.
inline constexpr int theme_style_count = 7;

enum class ThemeStyle { System, MacOS, Material, Win11, Win95, Plasma6, GNOME };

class Painter;

class Theme {
  public:
    virtual ~Theme() = default;
    static void add_theme_observer(std::function<void(const Theme &)> observer);
    static void notify_theme_changed();

    // FIXME this should be in the application, this method should be removed
    static const Theme &current();
    static void set_current(std::unique_ptr<Theme> theme);
    static ThemeStyle detect_system_style();
    static const char *style_name(ThemeStyle style);

    virtual Palette default_palette(ColorScheme scheme) const = 0;

  protected:
    static void init_fonts(Palette &p);

  public:
    virtual void draw_button(Painter &painter, Rect const &rect, std::string_view text,
                             Icon const &icon, WidgetState const &state, bool flat,
                             std::optional<Color> background = std::nullopt) const = 0;
    virtual void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text,
                               CheckState check_state, WidgetState const &state) const = 0;
    virtual void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text,
                                   bool checked, WidgetState const &state) const = 0;
    virtual void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                                 std::string_view placeholder, int cursor_pos, int selection_start,
                                 int selection_end, WidgetState const &state,
                                 bool password_mode = false, float scroll_offset = 0.0f,
                                 std::optional<Color> background = std::nullopt,
                                 bool cursor_visible = true, float right_inset = -1) const = 0;
    virtual void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title,
                                   bool hovered, bool active, bool show_mnemonics,
                                   int mnemonic_index) const = 0;
    virtual void draw_menubar_background(Painter &painter, Rect const &rect,
                                         WidgetState const &state) const = 0;
    virtual void draw_menu_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_menu_item(Painter &painter, Rect const &rect, std::string_view text,
                                Icon const &icon, std::string_view shortcut, bool hovered,
                                bool enabled, bool checkable, bool checked) const = 0;
    virtual void draw_menu_separator(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_menu_indicator(Painter &painter, Rect const &rect, bool enabled) const = 0;
    virtual void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                                   WidgetState const &state) const = 0;
    virtual void draw_slider(Painter &painter, Rect const &rect, float value, bool horizontal,
                             WidgetState const &state) const = 0;
    virtual void draw_scrollbar(Painter &painter, Rect const &rect, float value,
                                Orientation orientation, WidgetState const &state,
                                bool hovered_left_btn, bool pressed_left_btn,
                                bool hovered_right_btn, bool pressed_right_btn,
                                bool hovered_thumb) const = 0;
    virtual void draw_tab_bar_background(Painter &painter, Rect const &rect,
                                         WidgetState const &state) const = 0;
    virtual void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                          WidgetState const &state, TabOrientation orientation,
                          bool has_close = false, bool hovered_close = false) const = 0;
    virtual void draw_list_item(Painter &painter, Rect const &rect, std::string_view text,
                                Icon const &icon, bool selected, bool hovered,
                                bool alternate) const = 0;
    virtual void draw_list_background(Painter &painter, Rect const &rect,
                                      WidgetState const &state) const = 0;
    virtual void draw_table_background(Painter &painter, Rect const &rect,
                                       WidgetState const &state) const = 0;
    virtual void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                int depth, bool has_children, bool expanded, bool selected,
                                bool hovered, bool alternate) const = 0;
    virtual void draw_tree_background(Painter &painter, Rect const &rect,
                                      WidgetState const &state) const = 0;
    virtual void draw_icon_grid_item(Painter &painter, Rect const &rect, std::string_view text,
                                     Icon const &icon, bool selected, bool hovered, int icon_size,
                                     bool scale) const = 0;
    virtual void draw_combobox(Painter &painter, Rect const &rect, std::string_view text,
                               WidgetState const &state, bool open) const = 0;
    virtual void draw_combobox_item(Painter &painter, Rect const &rect, std::string_view text,
                                    bool hovered) const = 0;
    virtual void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const = 0;
    virtual void draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                                    WidgetState const &state) const = 0;
    virtual std::unique_ptr<Widget> create_title_bar(Window *window) const = 0;
    virtual void draw_tab_content_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_toolbar(Painter &painter, Rect const &rect,
                              WidgetState const &state) const = 0;
    virtual void draw_spinbox(Painter &painter, Rect const &rect, std::string_view text,
                              int cursor_pos, int selection_start, int selection_end,
                              WidgetState const &state, bool hovered_up, bool pressed_up,
                              bool hovered_down, bool pressed_down,
                              bool cursor_visible = true) const = 0;
    // FIXME: wtf is this chrono thingie? is it for cursor??
    virtual void draw_text_edit(Painter &painter, Rect const &rect,
                                std::span<std::string const> lines, int cursor_line, int cursor_col,
                                int selection_start_line, int selection_start_col,
                                int selection_end_line, int selection_end_col,
                                int first_visible_line, float line_height, float gutter_width,
                                float scroll_x, float scroll_y, WidgetState const &state,
                                std::chrono::steady_clock::time_point cursor_blink_time) const = 0;
    virtual void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const = 0;
    virtual void draw_focus_ring_for_widget(Painter &painter, Widget const *widget) const;
    virtual Size measure_label(std::string_view text, float font_size) const = 0;

    virtual Size measure_button(std::string_view text, Icon const &icon) const = 0;
    virtual Size measure_checkbox(std::string_view text) const = 0;
    virtual Size measure_radio_button(std::string_view text) const = 0;
    virtual Size measure_menubar_item(std::string_view text) const = 0;
    virtual Size measure_menu_item(std::string_view text, Icon const &icon,
                                   std::string_view shortcut) const = 0;
    virtual float menu_separator_height() const = 0;
    virtual Size measure_tab(std::string_view text) const = 0;
    virtual float list_item_height() const = 0;
    virtual Size measure_icon_grid_item(std::string_view text, int icon_size) const = 0;
    virtual Size measure_tooltip(std::string_view text) const = 0;

    virtual Margins button_padding() const = 0;
    virtual Margins line_input_padding() const = 0;

    std::string name;
    Palette palette;
    Style style;
};

} // namespace toolkit
