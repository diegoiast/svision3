// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/types.hpp"
#include "toolkit/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace toolkit {

class BaseTheme : public Theme {
  public:
    explicit BaseTheme(Palette p) {
        this->palette = std::move(p);
        // Backward compatibility: use old field names if new ones not set
        if (palette.window.r == 0 && palette.window.g == 0 && palette.window.b == 0) {
            if (palette.window_bg.r > 0 || palette.window_bg.g > 0 || palette.window_bg.b > 0) {
                palette.window = palette.window_bg;
            }
        }
        if (palette.base.r == 0 && palette.base.g == 0 && palette.base.b == 0) {
            if (palette.widget_bg.r > 0 || palette.widget_bg.g > 0 || palette.widget_bg.b > 0) {
                palette.base = palette.widget_bg;
            }
        }
        if (palette.fonts.font_size == 0 && palette.font_size > 0) {
            palette.fonts.font_size = palette.font_size;
        }

        // Initialize backward compatibility members
        name = "Base";
        style = ThemeStyle::Material;
        system_font = palette.fonts.system;
        monospace_font = palette.fonts.monospace;

        window.background = palette.window;

        // FIXME: all this should be removed. Custom colors are just a bad idea
        //        themes will not use these anyway.
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

        apply_base(button, palette);
        button.auto_repeat_delay = palette.fonts.auto_repeat_delay;
        button.auto_repeat_interval = palette.fonts.auto_repeat_interval;
        button.text_disabled = Color::mid(palette.text, palette.window);

        apply_base(line_input, palette);
        line_input.background = palette.base;
        line_input.background_focused = palette.base;
        line_input.border_focused = palette.accent;
        line_input.placeholder = Color::mid(palette.text, palette.base);
        line_input.cursor = palette.text;

        apply_base(text_edit, palette);
        text_edit.background = palette.base;
        text_edit.background_focused = palette.base;
        text_edit.border_focused = palette.accent;
        text_edit.placeholder = Color::mid(palette.text, palette.base);
        text_edit.cursor = palette.text;

        apply_base(checkbox, palette);
        checkbox.background = palette.base;
        checkbox.indicator = palette.accent;

        apply_base(radio, palette);
        radio.background = palette.base;
        radio.indicator = palette.accent;

        apply_base(combobox, palette);
        combobox.background = palette.base;
        combobox.border_focused = palette.accent;
        combobox.arrow =
            palette.text.luma() < 0.5f ? palette.text.darken(0.25f) : palette.text.lighten(0.25f);
        combobox.dropdown_bg = palette.base;
        combobox.item_hovered = palette.highlight;
        combobox.item_text_hovered = Color::with_gray(1.0f);

        apply_base(menu, palette);
        menu.background = palette.base;
        menu.background_hovered = palette.highlight;
        menu.item_hovered = palette.highlight;
        menu.item_text_hovered = Color::with_gray(1.0f);

        apply_base(menubar, palette);
        menubar.background = palette.window;
        menubar.background_hovered = palette.highlight;

        apply_base(tab_widget, palette);
        bool is_dark = palette.window.luma() < 0.5f;
        tab_widget.tab_active_bg = palette.window;
        tab_widget.tab_inactive_bg =
            is_dark ? palette.window.lighten(0.04f) : palette.window.darken(0.04f);
        tab_widget.tab_hover_bg =
            is_dark ? palette.window.lighten(0.08f) : palette.window.darken(0.02f);
        tab_widget.tab_active_text = palette.text;
        tab_widget.tab_inactive_text = Color::mid(
            palette.text, is_dark ? palette.window.lighten(0.20f) : palette.window.darken(0.20f));

        apply_base(list_view, palette);

        apply_base(tree_view, palette);
        tree_view.selected_bg = palette.accent;
        tree_view.selected_text = Color::with_gray(1.0f);
        tree_view.hovered_bg =
            is_dark ? palette.window.lighten(0.06f) : palette.window.darken(0.04f);
        tree_view.alternate_bg = palette.alternate;
        tree_view.indent = 20.0f;
        tree_view.background = Color::with_gray(1.0f);

        apply_base(progress_bar, palette);
        progress_bar.fill = palette.accent;

        apply_base(slider, palette);
        slider.groove = palette.border;
        slider.handle = palette.window;
        slider.handle_border = palette.border;

        layout.margins = {8, 8, 8, 8};
        layout.spacing = 8.0f;

        if (is_dark) {
            tooltip.background = Color::rgb(0.25f, 0.25f, 0.22f);
            tooltip.border = Color::rgb(0.45f, 0.45f, 0.40f);
            tooltip.text = Color::rgb(0.92f, 0.92f, 0.90f);
        } else {
            tooltip.background = Color::rgb(1.0f, 1.0f, 0.88f);
            tooltip.border = Color::rgb(0.6f, 0.6f, 0.5f);
            tooltip.text = Color::rgb(0.1f, 0.1f, 0.1f);
        }
        tooltip.font_size = palette.fonts.font_size - 1.0f;
    }

    void draw_button(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                     ButtonState state, bool focused, bool enabled, bool flat,
                     std::optional<Color> background) const override {
        auto const &style = button;
        auto hovered = state == ButtonState::Hovered || state == ButtonState::ClickedInside;
        auto pressed = state == ButtonState::ClickedInside;
        auto bg = background.value_or(style.background);
        auto border_c = focused ? style.border_focused : style.border;
        auto text_c = enabled ? style.text : style.text_disabled;
        auto text_offset = (style.beveled && pressed && enabled) ? 1.0f : 0.0f;
        auto fm = painter.font_metrics(style.font_size);
        auto text_w = painter.text_size(text, style.font_size).width;
        auto icon_w = 0.0f;
        auto icon_h = 0.0f;

        if (icon) {
            icon_w = static_cast<float>(icon->width);
            icon_h = static_cast<float>(icon->height);
        }
        auto total_w = text_w + (icon ? (icon_w + 4.0f) : 0.0f);
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_x = (rect.width - total_w) / 2.0f + text_offset + (icon ? icon_w + 4.0f : 0.0f);
        auto text_pos = Point{text_x, baseline_y + text_offset};

        if (enabled && !background) {
            if (pressed && style.background_pressed) {
                bg = *style.background_pressed;
            } else if (focused) {
                bg = style.background_selected;
            } else if (hovered && style.background_hovered) {
                bg = *style.background_hovered;
            }
        }

        bool show_full_frame = !flat || hovered || pressed;
        if (show_full_frame) {
            painter.draw_frame(rect, bg, border_c, style, pressed && enabled);
        } else if (style.corner_radius > 0.0f) {
            painter.fill_rounded_rect(rect, bg, style.corner_radius);
        } else {
            painter.fill_rect(rect, bg);
        }

        if (icon) {
            auto icon_x = (rect.width - total_w) / 2.0f + text_offset;
            auto icon_y = (rect.height - icon_h) / 2.0f;
            painter.draw_image(*icon, Point{icon_x, icon_y});
        }

        painter.draw_text(text, text_pos, text_c, style.font_size);
    }

    void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text,
                       CheckState check_state, ButtonState button_state, bool focused,
                       bool enabled) const override {
        auto const &style = checkbox;
        auto hovered =
            button_state == ButtonState::Hovered || button_state == ButtonState::ClickedInside;
        auto pressed = button_state == ButtonState::ClickedInside;
        auto fm = painter.font_metrics(style.font_size);
        auto box = style.box_size;
        auto box_y = (rect.height - box) / 2.0f;
        auto box_rect = Rect{rect.x, box_y, box, box};
        auto border = focused ? style.border_focused : style.border;
        auto bg = style.background;
        if (enabled) {
            if (pressed) {
                bg = bg.darken(0.08f);
            } else if (button_state == ButtonState::ClickedOutside) {
                bg = bg.darken(0.02f);
            } else if (hovered) {
                bg = bg.darken(0.04f);
            }
        }

        painter.draw_frame(box_rect, bg, border, style, pressed && enabled);

        if (check_state == CheckState::Checked) {
            auto cx = box_rect.x + box * 0.22f;
            auto cy = box_rect.y + box * 0.5f;
            auto lw = std::max(1.5f, box * 0.14f);
            painter.draw_line({cx, cy}, {cx + box * 0.18f, cy + box * 0.2f}, style.indicator, lw);
            painter.draw_line({cx + box * 0.18f, cy + box * 0.2f},
                              {cx + box * 0.55f, cy - box * 0.25f}, style.indicator, lw);
        } else if (check_state == CheckState::Partial) {
            auto gap = box * 0.25f;
            auto inner = box_rect.inset(gap);
            painter.fill_rect(inner, style.indicator);
        }

        auto text_x = rect.x + box + style.spacing;
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_c = enabled ? style.text : Color::mid(style.text, style.background);
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);
    }

    void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text, bool checked,
                           ButtonState button_state, bool focused, bool enabled) const override {
        auto const &style = radio;
        auto hovered =
            button_state == ButtonState::Hovered || button_state == ButtonState::ClickedInside;
        auto pressed = button_state == ButtonState::ClickedInside;
        auto fm = painter.font_metrics(style.font_size);
        auto r = style.box_size / 2.0f;
        auto center = Point{rect.x + r, rect.height / 2.0f};
        auto text_x = rect.x + style.box_size + style.spacing;
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto border = focused ? style.border_focused : style.border;
        auto bg = style.background;
        if (enabled) {
            if (pressed) {
                bg = bg.darken(0.08f);
            } else if (button_state == ButtonState::ClickedOutside) {
                bg = bg.darken(0.02f);
            } else if (hovered) {
                bg = bg.darken(0.04f);
            }
        }

        painter.fill_circle(center, r, bg);
        painter.draw_circle(center, r, border, style.border_width);

        if (style.beveled) {
            painter.draw_circle(center, r - 1.0f, style.shadow, 1.0f);
        }
        if (checked) {
            painter.fill_circle(center, r * 0.45f, style.indicator);
        }
        auto text_c = enabled ? style.text : Color::mid(style.text, style.background);
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);
    }

    void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                         std::string_view placeholder, int cursor_pos, int selection_start,
                         int selection_end, bool focused, bool enabled, bool password_mode,
                         float scroll_offset, std::optional<Color> background,
                         bool cursor_visible) const override {
        auto const &style = line_input;
        auto bg = background.value_or(focused ? style.background_focused : style.background);
        auto border = focused ? style.border_focused : style.border;
        auto fm = painter.font_metrics(style.font_size);
        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto content_x = style.padding.left;
        auto content_w = rect.width - style.padding.left - style.padding.right;
        auto tx = rect.x + content_x - scroll_offset;

        painter.draw_frame(rect, bg, border, style, true);

        auto clip_rect = Rect{rect.x + content_x, rect.y, content_w, rect.height};
        painter.push_clip(clip_rect);

        auto text_c = enabled ? style.text : Color::mid(style.text, style.background);

        if (selection_start >= 0 && selection_end > selection_start) {
            auto before_s = text.substr(0, selection_start);
            auto before_e = text.substr(0, selection_end);
            auto sx = tx + painter.text_size(before_s, style.font_size).width;
            auto ex = tx + painter.text_size(before_e, style.font_size).width;
            auto hy = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
            painter.fill_rect({sx, hy, ex - sx, fm.height + 2.0f}, sel_bg);
        }

        if (text.empty() && !focused) {
            painter.draw_text(placeholder, {tx, baseline_y}, style.placeholder, style.font_size);
        } else if (!text.empty()) {
            if (password_mode) {
                auto dot_radius = style.font_size * 0.25f;
                auto char_w = painter.text_size("8", style.font_size).width;
                auto center_off_y = (fm.ascent - fm.descent) / 2.0f;
                auto char_count = 0;
                auto i = 0;

                while (i < text.size()) {
                    auto cx = tx + char_count * char_w + char_w / 2.0f;
                    auto cy = baseline_y - center_off_y;
                    painter.fill_circle({cx, cy}, dot_radius, text_c);
                    char_count++;
                    i = Utf8Iterator::next(text, i);
                }
            } else {
                painter.draw_text(text, {tx, baseline_y}, text_c, style.font_size);
            }
        }

        if (focused && cursor_pos >= 0 && cursor_visible) {
            auto before = text.substr(0, cursor_pos);
            auto cx = tx;
            if (!before.empty()) {
                cx += painter.text_size(before, style.font_size).width;
            }
            auto cy_top = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto cy_bot = cy_top + fm.height + 2.0f;
            painter.draw_line({cx, cy_top}, {cx, cy_bot}, style.cursor, 1.5f);
        }

        painter.pop_clip();
    }

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
        auto border_c = window.background.darken(0.15f);
        painter.draw_line({rect.x, rect.height - 1.0f}, {rect.x + rect.width, rect.height - 1.0f},
                          border_c, 1.0f);
    }

    void draw_menu_background(Painter &painter, Rect const &rect) const override {
        auto const &style = combobox;
        auto shadow = Color::rgba(0, 0, 0, 0.12f);
        painter.fill_rounded_rect({rect.x + 1, rect.y + 1, rect.width, rect.height}, shadow,
                                  style.corner_radius);
        painter.fill_rounded_rect(rect, style.dropdown_bg, style.corner_radius);
        painter.draw_rounded_rect(rect, style.border, style.corner_radius, style.border_width);
    }

    void draw_menu_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        std::string_view shortcut, bool hovered, bool enabled, bool checkable,
                        bool checked) const override {
        auto const &style = combobox;
        auto fm = painter.font_metrics(style.font_size);
        auto baseline = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = style.text;

        if (hovered && enabled) {
            painter.fill_rounded_rect(rect, style.item_hovered, style.corner_radius * 0.5f);
        }

        if (checkable && checked) {
            auto check_rect = Rect{rect.x + 4, rect.y + (rect.height - 12) / 2, 12, 12};
            painter.fill_rounded_rect(check_rect, style.border, 2.0f);
        }

        auto icon_x = rect.x + style.padding.left + 4;
        if (icon) {
            auto icon_y = rect.y + (rect.height - static_cast<float>(icon->height)) / 2.0f;
            painter.draw_image(*icon, Point{icon_x, icon_y});
            icon_x += static_cast<float>(icon->width) + 4;
        }

        auto text_x = icon_x;
        if (!enabled) {
            text_col.a *= 0.4f;
        }
        painter.draw_text(text, {text_x, baseline}, text_col, style.font_size);

        if (!shortcut.empty()) {
            auto shortcut_w = painter.text_size(shortcut, style.font_size).width;
            auto shortcut_x = rect.x + rect.width - style.padding.right - shortcut_w - 10.0f;
            painter.draw_text(shortcut, {shortcut_x, baseline}, text_col, style.font_size);
        }
    }

    void draw_menu_separator(Painter &painter, Rect const &rect) const override {
        auto const &style = combobox;
        auto mid_y = rect.y + rect.height / 2.0f;
        auto sep_col = style.border;
        sep_col.a *= 0.5f;
        painter.draw_line({rect.x + 8, mid_y}, {rect.x + rect.width - 8, mid_y}, sep_col, 0.5f);
    }

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           bool enabled) const override {
        auto const &style = progress_bar;
        auto bg = enabled ? style.background : style.background.darken(0.1f);
        auto fill_c = enabled ? style.fill : style.fill.darken(0.2f);

        painter.draw_frame(rect, bg, style.border, style, true);

        auto inner = rect.inset(style.border_width);
        auto fill_w = inner.width * std::clamp(progress, 0.0f, 1.0f);
        auto fill_rect = Rect{inner.x, inner.y, fill_w, inner.height};

        if (style.chunked) {
            auto chunk_count =
                static_cast<int>(inner.width / (style.chunk_width + style.chunk_gap));
            for (int i = 0; i < chunk_count; ++i) {
                auto cx = inner.x + i * (style.chunk_width + style.chunk_gap);
                if (cx + style.chunk_width > inner.x + fill_w) {
                    break;
                }
                painter.fill_rect({cx, inner.y, style.chunk_width, inner.height}, fill_c);
            }
        } else {
            painter.fill_rect(fill_rect, fill_c);
        }
    }

    void draw_slider(Painter &painter, Rect const &rect, float value, bool horizontal, bool hovered,
                     bool pressed, bool focused, bool enabled) const override {
        auto const &style = slider;

        auto groove_rect = Rect{};
        auto handle_rect = Rect{};
        auto v = std::clamp(value, 0.0f, 1.0f);

        if (horizontal) {
            auto groove_y = rect.y + (rect.height - style.groove_thickness) / 2.0f;
            groove_rect = {rect.x, groove_y, rect.width, style.groove_thickness};
            auto track_len = rect.width - style.handle_size;
            auto handle_x = rect.x + style.handle_size / 2.0f + track_len * v;
            handle_rect = {handle_x - style.handle_size / 2.0f,
                           rect.y + (rect.height - style.handle_size) / 2.0f, style.handle_size,
                           style.handle_size};
        } else {
            auto groove_x = rect.x + (rect.width - style.groove_thickness) / 2.0f;
            groove_rect = {groove_x, rect.y, style.groove_thickness, rect.height};
            auto track_len = rect.height - style.handle_size;
            // Vertical slider: 0 is at bottom (local height - offset)
            // Slider::pos_to_value uses: offset = length - p - h_size / 2;
            // So p = length - h_size / 2 - offset
            // where offset = ratio * track_len
            auto handle_y = rect.y + rect.height - style.handle_size / 2.0f - track_len * v;
            handle_rect = {rect.x + (rect.width - style.handle_size) / 2.0f,
                           handle_y - style.handle_size / 2.0f, style.handle_size,
                           style.handle_size};
        }

        painter.fill_rounded_rect(groove_rect, style.groove, style.groove_thickness / 2.0f);

        auto bg = pressed ? style.handle.darken(0.1f)
                          : (hovered ? style.handle.lighten(0.1f) : style.handle);
        painter.fill_rounded_rect(handle_rect, bg, style.handle_size / 4.0f);
        painter.draw_rounded_rect(handle_rect, style.handle_border, style.handle_size / 4.0f, 1.0f);
    }

    void draw_tab_bar_background(Painter &painter, Rect const &rect) const override {
        auto const &style = tab_widget;
        painter.fill_rect(rect, style.tab_inactive_bg);
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto const &style = tab_widget;
        auto bg =
            active ? style.tab_active_bg : (hovered ? style.tab_hover_bg : style.tab_inactive_bg);
        auto text_c = active ? style.tab_active_text : style.tab_inactive_text;

        painter.fill_rect(rect, bg);

        auto vertical =
            (orientation == TabOrientation::West || orientation == TabOrientation::East);
        auto text_orientation = Painter::TextOrientation::Horizontal;
        if (orientation == TabOrientation::West) {
            text_orientation = Painter::TextOrientation::VerticalCW;
        } else if (orientation == TabOrientation::East) {
            text_orientation = Painter::TextOrientation::VerticalCCW;
        }

        auto fm = painter.font_metrics(style.font_size);
        auto text_w = painter.text_size(text, style.font_size).width;

        float text_x, baseline_y;
        if (vertical) {
            auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
            auto left_space = style.tab_padding_h;
            auto text_area_h = rect.height - left_space - right_space;
            text_x = rect.x + (rect.width - fm.height) / 2.0f;
            baseline_y = rect.y + left_space + (text_area_h - text_w) / 2.0f + fm.ascent;
            if (baseline_y < rect.y + left_space + fm.ascent) {
                baseline_y = rect.y + left_space + fm.ascent;
            }
        } else {
            auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
            auto left_space = style.tab_padding_h;
            auto text_area_w = rect.width - left_space - right_space;
            text_x = rect.x + left_space + (text_area_w - text_w) / 2.0f;
            if (text_x < rect.x + left_space) {
                text_x = rect.x + left_space;
            }
            baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        }

        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size, FontFamily::System,
                          text_orientation);

        if (has_close) {
            auto close_btn_size = 14.0f;
            auto close_x = rect.x + rect.width - style.tab_padding_h - close_btn_size;
            auto close_rect = Rect{close_x, rect.y + (rect.height - close_btn_size) / 2.0f,
                                   close_btn_size, close_btn_size};
            auto close_cy = rect.y + rect.height / 2.0f;
            auto close_cx = close_x + close_btn_size / 2.0f;

            if (hovered_close) {
                painter.fill_rounded_rect(close_rect, Color::rgb(0.9f, 0.2f, 0.2f), 4.0f);
            }

            auto cs = close_btn_size * 0.3f;
            auto x_col = hovered_close ? Color::rgb(1.0f, 1.0f, 1.0f)
                                       : Color::rgba(text_c.r, text_c.g, text_c.b, 0.6f);
            painter.draw_line({close_cx - cs, close_cy - cs}, {close_cx + cs, close_cy + cs}, x_col,
                              1.5f);
            painter.draw_line({close_cx + cs, close_cy - cs}, {close_cx - cs, close_cy + cs}, x_col,
                              1.5f);
        }
    }

    void draw_list_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        bool selected, bool hovered, bool alternate) const override {
        auto const &style = list_view;
        Color bg;

        auto is_dark = palette.window.luma() < 0.5f;
        auto alt_color = is_dark ? palette.base.lighten(0.03f) : palette.base.darken(0.02f);

        if (selected) {
            bg = palette.highlight;
        } else if (hovered) {
            bg = Color::lerp(alt_color, palette.highlight, 0.5);
        } else if (alternate) {
            bg = alt_color;
        } else {
            bg = palette.base;
        }

        painter.fill_rect(rect, bg);

        auto fm = painter.font_metrics(style.font_size);
        auto text_x = rect.x + style.item_padding_h;
        if (icon) {
            auto icon_y = rect.y + (rect.height - static_cast<float>(icon->height)) / 2.0f;
            painter.draw_image(*icon, {text_x, icon_y});
            text_x += static_cast<float>(icon->width) + style.item_padding;
        }
        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_c = selected ? palette.highlighted_text : palette.text;
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);
    }

    void draw_list_background(Painter &painter, Rect const &rect, bool focused) const override {
        auto const &style = list_view;
        if (style.beveled) {
            painter.draw_frame(rect, style.background, style.border, style, true);
        } else {
            painter.fill_rounded_rect(rect, style.background, style.corner_radius);
            if (style.border_width > 0) {
                painter.draw_rounded_rect(rect, style.border, style.corner_radius,
                                          style.border_width);
            }
        }
        if (focused) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
    }

    void draw_table_background(Painter &painter, Rect const &rect, bool focused) const override {
        auto const &style = table_view;
        if (style.beveled) {
            painter.draw_frame(rect, style.background, style.border, style, true);
        } else {
            painter.fill_rounded_rect(rect, style.background, style.corner_radius);
            if (style.border_width > 0) {
                painter.draw_rounded_rect(rect, style.border, style.corner_radius,
                                          style.border_width);
            }
        }
        if (focused) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
    }

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override {
        auto const &style = tree_view;
        auto fm = painter.font_metrics(style.font_size);

        auto x_offset = style.item_padding_h + depth * style.indent;

        if (has_children) {
            auto arrow_x = x_offset + 4;
            auto arrow_y = rect.y + rect.height / 2;
            auto arrow_size = 8.0f;

            if (expanded) {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size / 2, arrow_y + arrow_size / 2},
                                      style.text);
            } else {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x, arrow_y + arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y}, style.text);
            }
        }

        x_offset += style.indent + 4.0f;

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? style.selected_text : style.text;
        painter.draw_text(text, {x_offset, text_y}, text_col, style.font_size);
    }

    void draw_tree_background(Painter &painter, Rect const &rect, bool focused) const override {
        auto const &style = tree_view;
        if (style.beveled) {
            painter.draw_frame(rect, style.background, style.border, style, true);
        } else {
            painter.fill_rounded_rect(rect, style.background, style.corner_radius);
            if (style.border_width > 0) {
                painter.draw_rounded_rect(rect, style.border, style.corner_radius,
                                          style.border_width);
            }
        }
        if (focused) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
    }

    void draw_combobox(Painter &painter, Rect const &rect, std::string_view text, bool focused,
                       bool open) const override {
        auto const &style = combobox;
        auto border = focused ? style.border_focused : style.border;
        auto fm = painter.font_metrics(style.font_size);
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;

        painter.draw_frame(rect, style.background, border, style, true);

        if (!text.empty()) {
            auto clip_w = rect.width - style.padding.left - style.padding.right - 16.0f;
            painter.push_clip({style.padding.left, 0, clip_w, rect.height});
            painter.draw_text(text, {style.padding.left, baseline_y}, style.text, style.font_size);
            painter.pop_clip();
        }

        auto arrow_x = rect.width - style.padding.right - 8.0f;
        auto arrow_y = rect.height / 2.0f;
        auto aw = 4.0f;
        painter.draw_line({arrow_x - aw, arrow_y - 2.0f}, {arrow_x, arrow_y + 2.0f}, style.arrow,
                          1.5f);
        painter.draw_line({arrow_x, arrow_y + 2.0f}, {arrow_x + aw, arrow_y - 2.0f}, style.arrow,
                          1.5f);
    }

    void draw_combobox_item(Painter &painter, Rect const &rect, std::string_view text,
                            bool hovered) const override {
        auto const &style = combobox;
        auto fm = painter.font_metrics(style.font_size);
        auto baseline = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto tc = hovered ? style.item_text_hovered : style.text;

        if (hovered) {
            painter.fill_rect(rect, style.item_hovered);
        }

        painter.draw_text(text, {rect.x + style.padding.left, baseline}, tc, style.font_size);
    }

    void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const override {
        auto const &style = tooltip;

        if (style.corner_radius > 0.0f) {
            painter.fill_rounded_rect(rect, style.background, style.corner_radius);
            painter.draw_rounded_rect(rect, style.border, style.corner_radius, style.border_width);
        } else {
            painter.fill_rect(rect, style.background);
            painter.draw_rect(rect, style.border, style.border_width);
        }

        auto fm = painter.font_metrics(style.font_size);
        auto text_x = rect.x + style.padding;
        auto baseline_y = rect.y + style.padding + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, style.text, style.font_size);
    }

    void draw_toolbar(Painter &painter, Rect const &rect) const override {
        auto const &style = button;
        if (style.beveled) {
            painter.draw_line({rect.x, rect.y}, {rect.x + rect.width, rect.y}, style.highlight,
                              1.0f);
            painter.draw_line({rect.x, rect.y + rect.height - 1.0f},
                              {rect.x + rect.width, rect.y + rect.height - 1.0f}, style.shadow,
                              1.0f);
        } else {
            auto border_c = palette.window.darken(0.15f);
            painter.draw_line({rect.x, rect.y + rect.height - 1.0f},
                              {rect.x + rect.width, rect.y + rect.height - 1.0f}, border_c, 1.0f);
        }
    }

    void draw_spinbox(Painter &painter, Rect const &rect, std::string_view text, int cursor_pos,
                      int selection_start, int selection_end, bool focused, bool enabled,
                      bool hovered_up, bool pressed_up, bool hovered_down, bool pressed_down,
                      bool cursor_visible) const override {
        auto const &style = line_input;
        auto const &btn_style = button;
        auto bw = rect.height;
        auto field_rect = Rect{rect.x, rect.y, rect.width - bw, rect.height};
        auto bg = focused ? style.background_focused : style.background;
        auto border = focused ? style.border_focused : style.border;

        painter.draw_frame(field_rect, bg, border, style, true);

        auto content_x = field_rect.x + style.padding.left;
        auto content_w = field_rect.width - style.padding.left - style.padding.right;
        auto fm = painter.font_metrics(style.font_size);
        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;

        auto clip_rect = Rect{content_x, rect.y, content_w, rect.height};
        painter.push_clip(clip_rect);

        if (selection_start >= 0 && selection_end > selection_start) {
            auto before_s = text.substr(0, selection_start);
            auto before_e = text.substr(0, selection_end);
            auto sx = content_x + painter.text_size(before_s, style.font_size).width;
            auto ex = content_x + painter.text_size(before_e, style.font_size).width;
            auto hy = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
            painter.fill_rect({sx, hy, ex - sx, fm.height + 2.0f}, sel_bg);
        }

        auto text_c = enabled ? style.text : Color::mid(style.text, style.background);
        painter.draw_text(text, {content_x, baseline_y}, text_c, style.font_size);

        if (focused && cursor_pos >= 0 && cursor_visible) {
            auto before = text.substr(0, cursor_pos);
            auto cx = content_x;
            if (!before.empty()) {
                cx += painter.text_size(before, style.font_size).width;
            }
            auto cy_top = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto cy_bot = cy_top + fm.height + 2.0f;
            painter.draw_line({cx, cy_top}, {cx, cy_bot}, style.cursor, 1.5f);
        }

        painter.pop_clip();

        auto btn_w = bw;
        auto up_rect = Rect{rect.x + rect.width - btn_w, rect.y, btn_w, rect.height / 2.0f};
        auto down_rect = Rect{rect.x + rect.width - btn_w, rect.y + rect.height / 2.0f, btn_w,
                              rect.height / 2.0f};

        auto draw_spinbox_button = [&](Rect const &r, bool hovered, bool pressed) {
            auto b_bg = btn_style.background;
            if (pressed && btn_style.background_pressed) {
                b_bg = *btn_style.background_pressed;
            } else if (hovered && btn_style.background_hovered) {
                b_bg = *btn_style.background_hovered;
            }
            painter.draw_frame(r, b_bg, border, btn_style, false);

            auto cx = r.x + r.width / 2.0f;
            auto cy = r.y + r.height / 2.0f;
            auto arrow_sz = 3.5f;
            auto tc = style.text;

            if (r.y < rect.y + rect.height / 2.0f) {
                painter.draw_line({cx - arrow_sz, cy + arrow_sz * 0.4f}, {cx, cy - arrow_sz * 0.4f},
                                  tc, 1.5f);
                painter.draw_line({cx, cy - arrow_sz * 0.4f}, {cx + arrow_sz, cy + arrow_sz * 0.4f},
                                  tc, 1.5f);
            } else {
                painter.draw_line({cx - arrow_sz, cy - arrow_sz * 0.4f}, {cx, cy + arrow_sz * 0.4f},
                                  tc, 1.5f);
                painter.draw_line({cx, cy + arrow_sz * 0.4f}, {cx + arrow_sz, cy - arrow_sz * 0.4f},
                                  tc, 1.5f);
            }
        };

        draw_spinbox_button(up_rect, hovered_up, pressed_up);
        draw_spinbox_button(down_rect, hovered_down, pressed_down);
    }

    void draw_text_edit(Painter &painter, Rect const &rect, std::span<std::string const> lines,
                        int cursor_line, int cursor_col, int selection_start_line,
                        int selection_start_col, int selection_end_line, int selection_end_col,
                        int first_visible_line, float line_height, float gutter_width,
                        float scroll_x, float scroll_y, bool focused, bool enabled,
                        std::chrono::steady_clock::time_point cursor_blink_time) const override {
        auto const &style = text_edit;
        auto fm = painter.font_metrics(style.font_size, FontFamily::Monospace);
        auto bg = focused ? style.background_focused : style.background;
        auto border = focused ? style.border_focused : style.border;
        auto text_c = enabled ? style.text : Color::mid(style.text, style.background);

        painter.draw_frame(rect, bg, border, style, true);
        painter.push_clip(rect);

        auto last = std::min(static_cast<int>(lines.size()) - 1,
                             first_visible_line + static_cast<int>(rect.height / line_height));

        auto gutter_rect = Rect{rect.x, rect.y, gutter_width, rect.height};
        auto gutter_bg = style.background.darken(0.03f);
        painter.fill_rect(gutter_rect, gutter_bg);

        for (auto i = first_visible_line; i <= last; i++) {
            auto y = rect.y + line_height * static_cast<float>(i - first_visible_line);
            auto baseline = y + (line_height - fm.height) / 2.0f + fm.ascent;
            auto num = std::to_string(i + 1);
            auto nw = Painter::measure_text(num, style.font_size, FontFamily::Monospace).width;
            painter.draw_text(num, {rect.x + gutter_width - nw - 8.0f, baseline}, style.placeholder,
                              style.font_size, FontFamily::Monospace);
        }

        auto text_area =
            Rect{rect.x + gutter_width, rect.y, rect.width - gutter_width, rect.height};
        auto tx0 = rect.x + gutter_width - scroll_x;
        auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);

        auto has_sel = selection_start_line >= 0 && selection_end_line >= 0 &&
                       (selection_start_line < selection_end_line ||
                        (selection_start_line == selection_end_line &&
                         selection_start_col < selection_end_col));

        painter.push_clip(text_area);
        for (auto i = first_visible_line; i <= last; i++) {
            auto y = rect.y + line_height * static_cast<float>(i - first_visible_line);
            auto baseline = y + (line_height - fm.height) / 2.0f + fm.ascent;

            if (has_sel) {
                auto line_start_col = 0;
                auto line_end_col = static_cast<int>(lines[i].size());
                auto sel_start = (i == selection_start_line) ? selection_start_col : line_start_col;
                auto sel_end = (i == selection_end_line) ? selection_end_col : line_end_col;

                if (sel_start < sel_end) {
                    auto sx =
                        tx0 + (sel_start > 0
                                   ? Painter::measure_text(lines[i].substr(0, sel_start),
                                                           style.font_size, FontFamily::Monospace)
                                         .width
                                   : 0.0f);
                    auto ex = tx0 + Painter::measure_text(lines[i].substr(0, sel_end),
                                                          style.font_size, FontFamily::Monospace)
                                        .width;
                    if (i != selection_end_line) {
                        ex += style.font_size * 0.4f;
                    }
                    painter.fill_rect({sx, y, ex - sx, line_height}, sel_bg);
                }
            }

            painter.draw_text(lines[i], {tx0, baseline}, text_c, style.font_size,
                              FontFamily::Monospace);
        }

        if (focused) {
            auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            if ((ms / 500) % 2 == 0) {
                auto cy =
                    rect.y + line_height * static_cast<float>(cursor_line - first_visible_line);
                auto cx = tx0;
                if (cursor_col > 0 && cursor_line < static_cast<int>(lines.size())) {
                    cx += Painter::measure_text(lines[cursor_line].substr(0, cursor_col),
                                                style.font_size, FontFamily::Monospace)
                              .width;
                }
                painter.draw_line({cx, cy}, {cx, cy + line_height}, style.cursor, 1.5f);
            }
        }

        painter.pop_clip();

        auto content_h = line_height * static_cast<float>(lines.size());
        if (content_h > rect.height) {
            auto bar_h = std::max(20.0f, rect.height * (rect.height / content_h));
            auto bar_y = (scroll_y / content_h) * rect.height;
            auto sb = Rect{rect.x + rect.width - 6.0f, rect.y + bar_y, 4.0f, bar_h};
            painter.fill_rounded_rect(
                sb, Color::rgba(style.text.r, style.text.g, style.text.b, 0.25f), 2.0f);
        }

        painter.pop_clip();
    }

    void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const override {
        Color ring = line_input.border_focused;
        ring.a = 0.5f;
        float lw = 2.0f;
        float inset = lw / 2.0f + 0.5f;
        Rect r = rect.inset(inset);

        float dash_len = 2.0f;
        float gap_len = 2.0f;

        auto draw_dashed_line = [&](Point start, Point end) {
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.001f) {
                return;
            }
            float ux = dx / len;
            float uy = dy / len;
            float pos = 0.0f;
            bool drawing = true;
            while (pos < len) {
                float seg_len = drawing ? dash_len : gap_len;
                float next_pos = std::min(pos + seg_len, len);
                if (drawing) {
                    painter.draw_line({start.x + ux * pos, start.y + uy * pos},
                                      {start.x + ux * next_pos, start.y + uy * next_pos}, ring, lw);
                }
                pos = next_pos;
                drawing = !drawing;
            }
        };

        float cr = std::max(0.0f, corner_radius - inset);
        if (cr > 0.0f) {
            draw_dashed_line({r.x + cr, r.y}, {r.x + r.width - cr, r.y});
            draw_dashed_line({r.x + r.width, r.y + cr}, {r.x + r.width, r.y + r.height - cr});
            draw_dashed_line({r.x + r.width - cr, r.y + r.height}, {r.x + cr, r.y + r.height});
            draw_dashed_line({r.x, r.y + r.height - cr}, {r.x, r.y + cr});

            // Connect corners with diagonals
            draw_dashed_line({r.x + cr, r.y}, {r.x, r.y + cr});
            draw_dashed_line({r.x + r.width - cr, r.y}, {r.x + r.width, r.y + cr});
            draw_dashed_line({r.x + r.width, r.y + r.height - cr},
                             {r.x + r.width - cr, r.y + r.height});
            draw_dashed_line({r.x + cr, r.y + r.height}, {r.x, r.y + r.height - cr});
        } else {
            draw_dashed_line({r.x, r.y}, {r.x + r.width, r.y});
            draw_dashed_line({r.x + r.width, r.y}, {r.x + r.width, r.y + r.height});
            draw_dashed_line({r.x + r.width, r.y + r.height}, {r.x, r.y + r.height});
            draw_dashed_line({r.x, r.y + r.height}, {r.x, r.y});
        }
    }

    Size measure_label(std::string_view text, float font_size) const override {
        auto fm = Painter::measure_font_metrics(font_size);
        auto w = Painter::measure_text(text, font_size).width;
        return {w, fm.height + 4.0f};
    }

    Color error_color() const override { return palette.error.lighten(0.3f); }

    Size measure_button(std::string_view text, Icon const &icon) const override {
        auto text_w = Painter::measure_text(text, button.font_size).width;
        auto icon_w = 0.0f;
        if (icon) {
            icon_w = static_cast<float>(icon->width);
        }
        auto total_w = text_w + (icon ? icon_w + 4.0f : 0.0f);
        auto w = total_w + button.padding.left + button.padding.right;
        auto h = button.font_size + button.padding.top + button.padding.bottom;
        return {w, h};
    }

    Size measure_checkbox(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, checkbox.font_size).width;
        auto w = checkbox.box_size + checkbox.spacing + text_w;
        auto h = std::max(checkbox.box_size, checkbox.font_size);
        return Size{w, h};
    }

    Size measure_radio_button(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, radio.font_size).width;
        auto w = radio.box_size + radio.spacing + text_w;
        auto h = std::max(radio.box_size, radio.font_size);
        return Size{w, h};
    }

    Size measure_menubar_item(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, menubar.font_size).width;
        return {text_w + menubar.padding.left + menubar.padding.right, 0};
    }

    Size measure_menu_item(std::string_view text, Icon const &icon,
                           std::string_view shortcut) const override {
        auto w = menu.item_padding * 2;
        if (icon) {
            w += static_cast<float>(icon->width) + menu.item_padding;
        }
        w += Painter::measure_text(text, menu.font_size).width;
        if (!shortcut.empty()) {
            w += menu.item_padding + Painter::measure_text(shortcut, menu.font_size).width;
        }
        auto h = menu.font_size + menu.item_padding * 2;
        return {w, h};
    }

    float menu_separator_height() const override { return 8.0f; }
    Size measure_tab(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, tab_widget.font_size).width;
        auto w = text_w + tab_widget.tab_padding_h * 2;
        auto h = tab_widget.font_size + tab_widget.tab_padding_v * 2;
        return {w, h};
    }
    float list_item_height() const override { return 24.0f; }
    Size measure_tooltip(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, tooltip.font_size).width;
        auto w = text_w + tooltip.padding * 2;
        auto h = tooltip.font_size + tooltip.padding * 2;
        return {w, h};
    }

    Margins button_padding() const override { return button.padding; }
    Margins line_input_padding() const override { return line_input.padding; }
};

// ── Specific Themes ──────────────────────────────────────────────────────────

class MacOSTheme : public BaseTheme {
  public:
    explicit MacOSTheme(Palette p) : BaseTheme(std::move(p)) {
        name = "macOS";
        focus_ring_margin = 2.0f;
        focus_ring_corner_radius = 4.0f;
    }
};
class Win11Theme : public BaseTheme {
  public:
    explicit Win11Theme(Palette p) : BaseTheme(std::move(p)) {
        name = "Windows 11";
        focus_ring_margin = 3.0f;
        focus_ring_corner_radius = 4.0f;
    }

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
        auto text_c = style.text;
        if (hovered || active) {
            if (style.background_hovered.value_or(Color::rgb(0, 0, 0)).luma() < 0.5f) {
                text_c = Color::rgb(1, 1, 1);
            }
        }
        painter.draw_text(title, {rect.x + padding.left, baseline}, text_c, style.font_size);
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        BaseTheme::draw_tab(painter, rect, text, active, hovered, enabled, orientation, has_close,
                            hovered_close);

        if (active) {
            auto indicator = Rect{};
            auto lw = 2.0f;
            if (orientation == TabOrientation::North) {
                indicator = {rect.x + 4.0f, rect.y + rect.height - lw, rect.width - 8.0f, lw};
            } else if (orientation == TabOrientation::South) {
                indicator = {rect.x + 4.0f, rect.y, rect.width - 8.0f, lw};
            } else if (orientation == TabOrientation::West) {
                indicator = {rect.x + rect.width - lw, rect.y + 4.0f, lw, rect.height - 8.0f};
            } else if (orientation == TabOrientation::East) {
                indicator = {rect.x, rect.y, lw, rect.height};
            }
            painter.fill_rect(indicator, palette.accent);
        }
    }

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override {
        auto const &style = tree_view;
        auto fm = painter.font_metrics(style.font_size);
        auto indent = style.indent;

        auto x_offset = style.item_padding_h + depth * indent;

        if (has_children) {
            auto arrow_x = x_offset + 4;
            auto arrow_y = rect.y + rect.height / 2;
            auto arrow_size = 8.0f;

            if (expanded) {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y},
                                      {arrow_x, arrow_y + arrow_size / 2}, style.text);
            } else {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size / 2, arrow_y + arrow_size / 2},
                                      style.text);
            }
            x_offset += indent;
        }

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? style.selected_text : style.text;
        painter.draw_text(text, {x_offset, text_y}, text_col, style.font_size);
    }
};

class Win95Theme : public BaseTheme {
  public:
    explicit Win95Theme(Palette p) : BaseTheme(std::move(p)) {
        name = "Windows 95";
        progress_bar.chunked = true;
        progress_bar.bar_height = 20.0f;
        focus_ring_margin = 0.0f;
        focus_ring_corner_radius = 0.0f;
        focus_ring_line_style = Painter::LineStyle::Dotted;
    }

    void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const override {
        auto lw = 2.0f;
        auto dash_len = 2.0f;
        auto gap_len = 2.0f;
        auto x = rect.x;
        auto y = rect.y;
        auto w = rect.width;
        auto h = rect.height;
        auto ring = line_input.border_focused;
        ring.a = 0.5f;

        painter.draw_rect(rect, ring, lw);
        auto draw_dashed_line = [&](Point start, Point end) {
            auto dx = end.x - start.x;
            auto dy = end.y - start.y;
            auto len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.001f) {
                return;
            }
            auto ux = dx / len;
            auto uy = dy / len;
            auto pos = 0.0f;
            auto drawing = true;
            while (pos < len) {
                auto seg_len = drawing ? dash_len : gap_len;
                auto next_pos = std::min(pos + seg_len, len);
                if (drawing) {
                    painter.draw_line({start.x + ux * pos, start.y + uy * pos},
                                      {start.x + ux * next_pos, start.y + uy * next_pos}, ring, lw);
                }
                pos = next_pos;
                drawing = !drawing;
            }
        };

        if (corner_radius > 0.0f) {
            auto cr = std::max(0.0f, corner_radius);
            draw_dashed_line({x + cr, y}, {x + w - cr, y});
            draw_dashed_line({x + w, y + cr}, {x + w, y + h - cr});
            draw_dashed_line({x + w - cr, y + h}, {x + cr, y + h});
            draw_dashed_line({x, y + h - cr}, {x, y + cr});

            // Connect corners with diagonals
            draw_dashed_line({x + cr, y}, {x, y + cr});
            draw_dashed_line({x + w - cr, y}, {x + w, y + cr});
            draw_dashed_line({x + w, y + h - cr}, {x + w - cr, y + h});
            draw_dashed_line({x + cr, y + h}, {x, y + h - cr});
        } else {
            draw_dashed_line({x, y}, {x + w, y});
            draw_dashed_line({x + w, y}, {x + w, y + h});
            draw_dashed_line({x + w, y + h}, {x, y + h});
            draw_dashed_line({x, y + h}, {x, y});
        }
    }

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override {
        auto const &style = tree_view;
        auto const &cb_style = checkbox;
        auto fm = painter.font_metrics(style.font_size);
        auto x_offset = style.item_padding_h + depth * style.indent;

        if (has_children) {
            auto icon_x = x_offset;
            auto box_size = cb_style.box_size;
            auto box_rect =
                Rect{icon_x, rect.y + (rect.height - box_size) / 2.0f, box_size, box_size};

            painter.draw_frame(box_rect, cb_style.background, cb_style.border, cb_style, false);

            auto expand_collapse_char = expanded ? "-" : "+";
            auto char_w = painter.text_size(expand_collapse_char, style.font_size).width;
            auto char_x = icon_x + (box_size - char_w) / 2.0f;
            auto char_y = box_rect.y + (box_size - fm.height) / 2.0f + fm.ascent;

            painter.draw_text(expand_collapse_char, Point{char_x, char_y}, style.text,
                              style.font_size);
        }

        x_offset += style.indent + 4.0f;

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? style.selected_text : style.text;
        painter.draw_text(text, Point{x_offset, text_y}, text_col, style.font_size);
    }

    void draw_tree_background(Painter &painter, Rect const &rect, bool focused) const override {
        auto const &style = tree_view;
        if (style.beveled) {
            painter.draw_frame(rect, style.background, style.border, style, true);
        } else {
            painter.fill_rounded_rect(rect, style.background, style.corner_radius);
            if (style.border_width > 0) {
                painter.draw_rounded_rect(rect, style.border, style.corner_radius,
                                          style.border_width);
            }
        }
        if (focused) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
    }
};

class MaterialTheme : public BaseTheme {
  public:
    explicit MaterialTheme(Palette p) : BaseTheme(std::move(p)) {
        auto is_dark = palette.window.luma() < 0.5f;
        name = "Material";
        button.background_hovered =
            is_dark ? palette.window.lighten(0.08f) : palette.window.darken(0.04f);
        button.background_pressed =
            is_dark ? palette.window.lighten(0.15f) : palette.window.darken(0.10f);
        button.padding = {10, 24, 10, 24};
        button.corner_radius = 4.0f;
        menu.padding = {4, 4, 4, 4};
        menubar.padding = {4, 12, 4, 12};
        slider.handle_size = 18.0f;
        slider.groove_thickness = 4.0f;
        focus_ring_margin = 3.0f;
        focus_ring_corner_radius = 3.0f;
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto const &style = tab_widget;
        auto bg =
            active ? style.tab_active_bg : (hovered ? style.tab_hover_bg : style.tab_inactive_bg);
        auto text_c = active ? style.tab_active_text : style.tab_inactive_text;

        painter.fill_rect(rect, bg);

        auto fm = painter.font_metrics(style.font_size);
        auto text_w = painter.text_size(text, style.font_size).width;

        auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
        auto left_space = style.tab_padding_h;
        auto text_area_w = rect.width - left_space - right_space;
        auto text_x = rect.x + left_space + (text_area_w - text_w) / 2.0f;
        if (text_x < rect.x + left_space) {
            text_x = rect.x + left_space;
        }

        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);

        if (has_close) {
            auto close_btn_size = 14.0f;
            auto close_x = rect.x + rect.width - style.tab_padding_h - close_btn_size;
            auto close_rect = Rect{close_x, rect.y + (rect.height - close_btn_size) / 2.0f,
                                   close_btn_size, close_btn_size};
            auto close_cy = rect.y + rect.height / 2.0f;
            auto close_cx = close_x + close_btn_size / 2.0f;

            if (hovered_close) {
                painter.fill_rounded_rect(close_rect, Color::rgb(0.9f, 0.2f, 0.2f), 4.0f);
            }

            auto cs = close_btn_size * 0.3f;
            auto x_col = hovered_close ? Color::rgb(1.0f, 1.0f, 1.0f)
                                       : Color::rgba(text_c.r, text_c.g, text_c.b, 0.6f);
            painter.draw_line({close_cx - cs, close_cy - cs}, {close_cx + cs, close_cy + cs}, x_col,
                              1.5f);
            painter.draw_line({close_cx + cs, close_cy - cs}, {close_cx - cs, close_cy + cs}, x_col,
                              1.5f);
        }

        if (active) {
            auto indicator = Rect{};
            float lw = 2.0f;
            if (orientation == TabOrientation::North) {
                indicator = {rect.x, rect.y + rect.height - lw, rect.width, lw};
            } else if (orientation == TabOrientation::South) {
                indicator = {rect.x, rect.y, rect.width, lw};
            } else if (orientation == TabOrientation::West) {
                indicator = {rect.x + rect.width - lw, rect.y, lw, rect.height};
            } else if (orientation == TabOrientation::East) {
                indicator = {rect.x, rect.y, lw, rect.height};
            }
            painter.fill_rect(indicator, palette.accent);
        }
    }
};

class GnomeTheme : public BaseTheme {
  public:
    explicit GnomeTheme(Palette p) : BaseTheme(std::move(p)) {
        name = "GNOME";
        bool dark = palette.window.luma() < 0.5f;
        Color btn_bg = dark ? palette.window.lighten(0.04f) : palette.window.darken(0.03f);
        button.background = btn_bg;
        button.background_hovered = dark ? btn_bg.lighten(0.04f) : btn_bg.darken(0.04f);
        button.background_pressed = dark ? btn_bg.darken(0.06f) : btn_bg.darken(0.10f);
        button.border = dark ? palette.border.lighten(0.04f) : palette.border.darken(0.06f);
        button.padding = {8, 20, 8, 20};
        button.corner_radius = 8.0f;

        checkbox.corner_radius = 5.0f;
        checkbox.border_width = 2.0f;
        radio.border_width = 2.0f;
        line_input.corner_radius = palette.corner_radius;
        combobox.corner_radius = palette.corner_radius;
        slider.handle_size = 22.0f;
        slider.groove_thickness = 6.0f;

        focus_ring_margin = 2.0f;
        focus_ring_corner_radius = 2.0f;
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto const &style = tab_widget;
        auto bg =
            active ? style.tab_active_bg : (hovered ? style.tab_hover_bg : style.tab_inactive_bg);
        if (!active && !hovered) {
            bg = bg.darken(0.05f);
        }
        auto text_c = active ? style.tab_active_text : style.tab_inactive_text;

        auto tab_rect = rect.inset(2.0f);
        painter.fill_rounded_rect(tab_rect, bg, 6.0f);

        auto fm = painter.font_metrics(style.font_size);
        auto text_w = painter.text_size(text, style.font_size).width;

        auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
        auto left_space = style.tab_padding_h;
        auto text_area_w = rect.width - left_space - right_space;
        auto text_x = rect.x + left_space + (text_area_w - text_w) / 2.0f;
        if (text_x < rect.x + left_space) {
            text_x = rect.x + left_space;
        }

        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);

        if (has_close) {
            auto close_btn_size = 14.0f;
            auto close_x = rect.x + rect.width - style.tab_padding_h - close_btn_size - 2.0f;
            auto close_rect = Rect{close_x, rect.y + (rect.height - close_btn_size) / 2.0f,
                                   close_btn_size, close_btn_size};
            auto close_cy = rect.y + rect.height / 2.0f;
            auto close_cx = close_x + close_btn_size / 2.0f;

            if (hovered_close) {
                painter.fill_circle({close_cx, close_cy}, close_btn_size / 2.0f + 2.0f,
                                    Color::rgba(text_c.r, text_c.g, text_c.b, 0.15f));
            }

            auto cs = close_btn_size * 0.3f;
            auto x_col = Color::rgba(text_c.r, text_c.g, text_c.b, 0.7f);
            painter.draw_line({close_cx - cs, close_cy - cs}, {close_cx + cs, close_cy + cs}, x_col,
                              1.5f);
            painter.draw_line({close_cx + cs, close_cy - cs}, {close_cx - cs, close_cy + cs}, x_col,
                              1.5f);
        }
    }
};

class Plasma6Theme : public BaseTheme {
  public:
    explicit Plasma6Theme(Palette p) : BaseTheme(std::move(p)) {
        name = "Plasma 6";
        button.padding = {6, 18, 6, 18};
        button.corner_radius = 5.0f;
        button.background_hovered = palette.highlight;
        button.background_pressed = palette.highlight.darken(0.1f);
        checkbox.corner_radius = 5.0f;
        checkbox.indicator = Color::with_gray(0.0f);
        radio.indicator = Color::with_gray(0.0f);
        slider.handle_size = 20.0f;
        slider.groove_thickness = 6.0f;
        focus_ring_margin = 2.0f;
        focus_ring_corner_radius = 4.0f;
    }

    void draw_button(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                     ButtonState state, bool focused, bool enabled, bool flat,
                     std::optional<Color> background) const override {
        BaseTheme::draw_button(painter, rect, text, icon, state, focused, enabled, flat,
                               background);

        auto pressed = state == ButtonState::ClickedInside;
        if (enabled && !flat && !pressed) {
            auto line_c = palette.border;
            line_c.a = 0.3f;
            painter.draw_line(
                {rect.x + button.corner_radius, rect.y + rect.height - 2.0f},
                {rect.x + rect.width - button.corner_radius, rect.y + rect.height - 2.0f}, line_c,
                1.0f);
        }
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        BaseTheme::draw_tab(painter, rect, text, active, hovered, enabled, orientation, has_close,
                            hovered_close);

        if (active) {
            auto indicator = Rect{};
            float lw = 2.0f;
            if (orientation == TabOrientation::North) {
                indicator = {rect.x, rect.y + rect.height - lw, rect.width, lw};
            } else if (orientation == TabOrientation::South) {
                indicator = {rect.x, rect.y, rect.width, lw};
            } else if (orientation == TabOrientation::West) {
                indicator = {rect.x + rect.width - lw, rect.y, lw, rect.height};
            } else if (orientation == TabOrientation::East) {
                indicator = {rect.x, rect.y, lw, rect.height};
            }
            painter.fill_rect(indicator, palette.accent);
        }
    }

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override {
        auto const &style = tree_view;
        auto fm = painter.font_metrics(style.font_size);
        auto indent = style.indent;

        auto x_offset = style.item_padding_h + depth * indent;

        if (has_children) {
            auto center_x = x_offset + indent / 2;
            auto arrow_y = rect.y + rect.height / 2;
            auto arrow_size = 8.0f;
            auto arrow_offset = arrow_size * 0.3f;

            if (expanded) {
                painter.draw_line({center_x - arrow_size / 2, arrow_y - arrow_offset},
                                  {center_x, arrow_y + arrow_size / 2}, style.text, 1.5f);
                painter.draw_line({center_x, arrow_y + arrow_size / 2},
                                  {center_x + arrow_size / 2, arrow_y - arrow_offset}, style.text,
                                  1.5f);
            } else {
                painter.draw_line({center_x - arrow_offset, arrow_y - arrow_size / 2},
                                  {center_x + arrow_offset, arrow_y}, style.text, 1.5f);
                painter.draw_line({center_x - arrow_offset, arrow_y + arrow_size / 2},
                                  {center_x + arrow_offset, arrow_y}, style.text, 1.5f);
            }
        }

        x_offset += indent + 4.0f;

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? style.selected_text : style.text;
        painter.draw_text(text, {x_offset, text_y}, text_col, style.font_size);
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
        p.window = Color::with_gray(0.93f);
        p.base = Color::with_gray(1.0f);
        p.text = Color::with_gray(0.20f);
        p.border = Color::with_gray(0.75f);
        p.alternate = Color::with_gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window = Color::with_gray(0.18f);
        p.base = Color::with_gray(0.24f);
        p.text = Color::with_gray(0.92f);
        p.border = Color::with_gray(0.38f);
        p.alternate = Color::with_gray(0.28f);
        break;
    }
}

static void palette_win11(Palette &p, ColorScheme scheme) {
    // Default Windows 11 accent (approx system blue)
    Color winBlue = Color::rgb(0.0f, 0.47f, 0.84f);
    p.accent = winBlue;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::rgb(0.95f, 0.95f, 0.95f);      // #F2F2F2
        p.base = Color::rgb(1.0f, 1.0f, 1.0f);           // #FFFFFF
        p.alternate = Color::rgb(0.97f, 0.97f, 0.97f);   // subtle alt row
        p.text = Color::rgb(0.10f, 0.10f, 0.10f);        // near-black
        p.placeholder = Color::rgb(0.55f, 0.55f, 0.55f); // muted gray
        p.highlight = winBlue;
        p.highlighted_text = Color::rgb(1.0f, 1.0f, 1.0f); // white on accent
        p.border = Color::rgb(0.85f, 0.85f, 0.85f);        // light divider
        p.shadow = Color::rgb(0.0f, 0.0f, 0.15f);          // soft shadow
        p.dark_shadow = Color::rgb(0.0f, 0.0f, 0.30f);
        p.link = Color::rgb(0.0f, 0.40f, 0.85f);     // Win-style link blue
        p.success = Color::rgb(0.06f, 0.53f, 0.26f); // green 600-ish
        p.warning = Color::rgb(0.96f, 0.64f, 0.0f);  // amber
        p.error = Color::rgb(0.75f, 0.16f, 0.18f);   // red toned (not pure)

        break;

    case ColorScheme::Dark:
        p.window = Color::rgb(0.12f, 0.12f, 0.12f); // #1F1F1F-ish
        p.base = Color::rgb(0.16f, 0.16f, 0.16f);   // surfaces
        p.alternate = Color::rgb(0.20f, 0.20f, 0.20f);
        p.text = Color::rgb(0.95f, 0.95f, 0.95f);
        p.placeholder = Color::rgb(0.65f, 0.65f, 0.65f);
        p.highlight = winBlue;
        p.highlighted_text = Color::rgb(1.0f, 1.0f, 1.0f);
        p.border = Color::rgb(0.30f, 0.30f, 0.30f); // subtle edge
        p.shadow = Color::rgb(0.0f, 0.0f, 0.50f);
        p.dark_shadow = Color::rgb(0.0f, 0.0f, 0.80f);
        p.link = Color::rgb(0.25f, 0.62f, 1.0f); // brighter for dark
        p.success = Color::rgb(0.30f, 0.75f, 0.40f);
        p.warning = Color::rgb(1.0f, 0.78f, 0.30f);
        p.error = Color::rgb(1.0f, 0.45f, 0.45f);
        break;
    }
}

static void palette_material(Palette &p, ColorScheme scheme) {
    auto matPurple = Color::rgb(0.384f, 0.0f, 0.933f);
    p.corner_radius = 4.0f;
    p.accent = matPurple;
    p.highlight = matPurple;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::with_gray(0.98f);
        p.base = Color::with_gray(1.0f);
        p.text = Color::with_gray(0.13f);
        p.border = Color::with_gray(0.74f);
        p.alternate = Color::with_gray(0.90f);
        p.highlighted_text = Color::rgb(1.0f, 1.0f, 1.0f); // white on accent
        break;
    case ColorScheme::Dark:
        p.window = Color::rgb(0.07f, 0.07f, 0.07f);
        p.base = Color::rgb(0.12f, 0.12f, 0.12f);
        p.text = Color::with_gray(0.93f);
        p.border = Color::with_gray(0.33f);
        p.accent = Color::rgb(0.55f, 0.33f, 0.97f);
        p.highlight = p.accent;
        p.alternate = Color::rgb(0.16f, 0.16f, 0.16f);
        p.highlighted_text = Color::rgb(1.0f, 1.0f, 1.0f); // white on accent
        break;
    }
}

static void palette_win95(Palette &p, ColorScheme scheme) {
    p.beveled = true;
    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::with_gray(0.75f);
        p.base = Color::with_gray(1.0f);
        p.text = Color::with_gray(0.0f);
        p.border = Color::with_gray(0.0f);
        p.accent = Color::rgb(0.0f, 0.0f, 0.5f);
        p.highlight = Color::rgb(0.0f, 0.0f, 0.5f);
        p.highlighted_text = Color::rgb(1.0f, 1.0f, 1.5f);
        p.shadow = Color::with_gray(0.50f);
        p.dark_shadow = Color::with_gray(0.0f);
        p.alternate = Color::with_gray(0.90f);

        p.error = Color::rgb(0.8, 0.2, 0.2);
        p.warning = Color::rgb(0.8, 0.7, 0.0);
        p.success = Color::rgb(0.0, 0.2, 0.8);
        break;
    case ColorScheme::Dark:
        p.window = Color::with_gray(0.25f);
        p.base = Color::with_gray(0.10f);
        p.text = Color::with_gray(0.90f);
        p.border = Color::with_gray(0.10f);
        p.accent = Color::rgb(0.0f, 0.0f, 0.8f);
        p.highlight = Color::with_gray(0.40f);
        p.shadow = Color::with_gray(0.12f);
        p.dark_shadow = Color::with_gray(0.0f);
        p.alternate = Color::with_gray(0.35f);

        p.error = Color::rgb(0.8, 0.0, 0.0);
        p.warning = Color::rgb(0.8, 0.7, 0.0);
        p.success = Color::rgb(0.0, 0.2, 0.8);
        break;
    }
}

static void palette_plasma6(Palette &p, ColorScheme scheme) {
    p.corner_radius = 5.0f;
    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFeff0f1);
        p.base = Color::with_gray(1.0f);
        p.text = Color::rgb(0.137f, 0.149f, 0.161f);
        p.border = Color::rgb(0.737f, 0.753f, 0.773f);
        p.accent = Color::from_argb(0xFF3daee9);
        p.highlight = Color::from_argb(0xFFd6ecf8);
        p.alternate = Color::with_gray(0.90f);
        p.error = Color::rgb(0.9, 0.3, 0.3);
        break;
    case ColorScheme::Dark:
        p.window = Color::rgb(0.137f, 0.149f, 0.161f);
        p.base = Color::rgb(0.192f, 0.212f, 0.231f);
        p.text = Color::rgb(0.937f, 0.941f, 0.945f);
        p.border = Color::with_gray(0.30f);
        p.accent = Color::rgb(0.239f, 0.682f, 0.914f);
        p.highlight = p.accent;
        p.alternate = Color::rgb(0.23f, 0.25f, 0.27f);
        break;
    }
}

static void palette_gnome(Palette &p, ColorScheme scheme) {
    p.corner_radius = 8.0f;
    Color gnomeBlue = Color::rgb(0.21f, 0.52f, 0.89f);
    p.accent = gnomeBlue;
    p.highlight = gnomeBlue;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::rgb(0.98f, 0.98f, 0.98f);
        p.base = Color::with_gray(1.0f);
        p.text = Color::rgb(0.18f, 0.20f, 0.21f);
        p.border = Color::rgb(0.86f, 0.84f, 0.83f);
        p.alternate = Color::with_gray(0.90f);
        break;
    case ColorScheme::Dark:
        p.window = Color::rgb(0.14f, 0.14f, 0.14f);
        p.base = Color::rgb(0.22f, 0.22f, 0.22f);
        p.text = Color::with_gray(0.95f);
        p.border = Color::with_gray(0.30f);
        p.alternate = Color::rgb(0.26f, 0.26f, 0.26f);
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
    auto corner_radius = line_input.corner_radius + focus_ring_corner_radius;

    painter.set_line_style(focus_ring_line_style);
    draw_focus_ring(painter, r, corner_radius);
    painter.set_line_style(Painter::LineStyle::Solid);
}

std::unique_ptr<Theme> Theme::create(ThemeStyle style, ColorScheme scheme) {
    return create(style, default_palette(style, scheme));
}

std::unique_ptr<Theme> Theme::create(ThemeStyle style, Palette const &palette) {
    std::unique_ptr<Theme> t;
    switch (style) {
    case ThemeStyle::MacOS:
        t = std::make_unique<MacOSTheme>(palette);
        break;
    case ThemeStyle::Material:
        t = std::make_unique<MaterialTheme>(palette);
        break;
    case ThemeStyle::Win11:
        t = std::make_unique<Win11Theme>(palette);
        break;
    case ThemeStyle::Win95:
        t = std::make_unique<Win95Theme>(palette);
        break;
    case ThemeStyle::Plasma6:
        t = std::make_unique<Plasma6Theme>(palette);
        break;
    case ThemeStyle::GNOME:
        t = std::make_unique<GnomeTheme>(palette);
        break;
    default:
        t = std::make_unique<BaseTheme>(palette);
        break;
    }

    t->name = style_name(style);
    t->style = style;
    t->palette = palette;
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
