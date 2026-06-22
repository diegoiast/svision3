// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <toolkit/theme.hpp>

namespace toolkit {

class BaseTheme : public Theme {
  public:
    explicit BaseTheme(ColorScheme scheme = ColorScheme::Light,
                       std::optional<Palette> p = std::nullopt);

    Palette default_palette(ColorScheme scheme) const override;

    void draw_button(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                     WidgetState const &state, bool flat,
                     std::optional<Color> background) const override;

    void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text,
                       CheckState check_state, WidgetState const &state) const override;

    void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text, bool checked,
                           WidgetState const &state) const override;

    void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                         std::string_view placeholder, int cursor_pos, int selection_start,
                         int selection_end, WidgetState const &state, bool password_mode,
                         float scroll_offset, std::optional<Color> background, bool cursor_visible,
                         float right_inset = -1, FontFamily font_family = FontFamily::System)
        const override;
    void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title, bool hovered,
                           bool active, bool show_mnemonics, int mnemonic_index) const override;
    void draw_menubar_background(Painter &painter, Rect const &rect,
                                 WidgetState const &state) const override;
    void draw_menu_background(Painter &painter, Rect const &rect) const override;
    void draw_menu_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        std::string_view shortcut, bool hovered, bool enabled, bool checkable,
                        bool checked) const override;
    void draw_menu_separator(Painter &painter, Rect const &rect) const override;
    void draw_menu_indicator(Painter &painter, Rect const &rect, bool enabled) const override;

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           WidgetState const &state) const override;

    void draw_slider(Painter &painter, Rect const &rect, float value, bool horizontal,
                     WidgetState const &state) const override;
    void draw_scrollbar(Painter &painter, Rect const &rect, float value, Orientation orientation,
                        WidgetState const &state, bool hovered_left_btn, bool pressed_left_btn,
                        bool hovered_right_btn, bool pressed_right_btn,
                        bool hovered_thumb) const override;

    void draw_tab_bar_background(Painter &painter, Rect const &rect,
                                 WidgetState const &state) const override;
    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  WidgetState const &state, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override;
    void draw_list_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        bool selected, bool hovered, bool alternate) const override;
    void draw_list_background(Painter &painter, Rect const &rect,
                              WidgetState const &state) const override;
    void draw_table_background(Painter &painter, Rect const &rect,
                               WidgetState const &state) const override;
    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override;
    void draw_tree_background(Painter &painter, Rect const &rect,
                              WidgetState const &state) const override;
    void draw_icon_grid_item(Painter &painter, Rect const &rect, std::string_view text,
                             Icon const &icon, bool selected, bool hovered, int icon_size,
                             bool scale) const override;
    void draw_combobox(Painter &painter, Rect const &rect, std::string_view text,
                       WidgetState const &state, bool open) const override;
    void draw_combobox_item(Painter &painter, Rect const &rect, std::string_view text,
                            bool hovered) const override;

    void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const override;
    void draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                            WidgetState const &state) const override;
    void draw_tab_content_background(Painter &painter, Rect const &rect) const override;
    std::unique_ptr<Widget> create_title_bar(Window *window) const override;

  protected:
    void draw_toolbar(Painter &painter, Rect const &rect, WidgetState const &state) const override;
    void draw_spinbox(Painter &painter, Rect const &rect, std::string_view text, int cursor_pos,
                      int selection_start, int selection_end, WidgetState const &state,
                      bool hovered_up, bool pressed_up, bool hovered_down, bool pressed_down,
                      bool cursor_visible) const override;
    void draw_text_edit(Painter &painter, Rect const &rect, std::span<std::string const> lines,
                        int cursor_line, int cursor_col, int selection_start_line,
                        int selection_start_col, int selection_end_line, int selection_end_col,
                        int first_visible_line, float line_height, float gutter_width,
                        float scroll_x, float scroll_y, WidgetState const &state,
                        std::chrono::steady_clock::time_point cursor_blink_time) const override;

    void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const override;

    Size measure_label(std::string_view text, float font_size) const override;
    Size measure_button(std::string_view text, Icon const &icon) const override;
    Size measure_checkbox(std::string_view text) const override;
    Size measure_radio_button(std::string_view text) const override;
    Size measure_menubar_item(std::string_view text) const override;
    Size measure_menu_item(std::string_view text, Icon const &icon,
                           std::string_view shortcut) const override;
    float menu_separator_height() const override;
    Size measure_tab(std::string_view text) const override;
    float list_item_height() const override;
    Size measure_icon_grid_item(std::string_view text, int icon_size) const override;
    Size measure_tooltip(std::string_view text) const override;
    Margins button_padding() const override;
    Margins line_input_padding() const override;
};

} // namespace toolkit
