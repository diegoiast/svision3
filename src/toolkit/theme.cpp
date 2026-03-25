// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/utf8.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <spdlog/spdlog.h>

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
        // Backward compatibility: use old field names if new ones not set
        if (palette_.window.r == 0 && palette_.window.g == 0 && palette_.window.b == 0) {
            if (palette_.window_bg.r > 0 || palette_.window_bg.g > 0 || palette_.window_bg.b > 0) {
                palette_.window = palette_.window_bg;
            }
        }
        if (palette_.base.r == 0 && palette_.base.g == 0 && palette_.base.b == 0) {
            if (palette_.widget_bg.r > 0 || palette_.widget_bg.g > 0 || palette_.widget_bg.b > 0) {
                palette_.base = palette_.widget_bg;
            }
        }
        if (palette_.fonts.font_size == 0 && palette_.font_size > 0) {
            palette_.fonts.font_size = palette_.font_size;
        }

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
                     bool hovered, bool pressed, bool focused, bool enabled, bool flat,
                     std::optional<Color> background) const override {
        auto const &style = button;
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

        if (focused && enabled) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
    }

    void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text, CheckState state,
                       bool hovered, bool pressed, bool focused, bool enabled) const override {
        auto const &style = checkbox;
        auto fm = painter.font_metrics(style.font_size);
        auto box = style.box_size;
        auto box_y = (rect.height - box) / 2.0f;
        auto box_rect = Rect{rect.x, box_y, box, box};
        auto border = focused ? style.border_focused : style.border;

        painter.draw_frame(box_rect, style.background, border, style, true);

        if (state == CheckState::Checked) {
            auto cx = box_rect.x + box * 0.22f;
            auto cy = box_rect.y + box * 0.5f;
            auto lw = std::max(1.5f, box * 0.14f);
            painter.draw_line({cx, cy}, {cx + box * 0.18f, cy + box * 0.2f}, style.indicator, lw);
            painter.draw_line({cx + box * 0.18f, cy + box * 0.2f},
                              {cx + box * 0.55f, cy - box * 0.25f}, style.indicator, lw);
        } else if (state == CheckState::Partial) {
            auto gap = box * 0.25f;
            auto inner = box_rect.inset(gap);
            painter.fill_rect(inner, style.indicator);
        }

        auto text_x = rect.x + box + style.spacing;
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_c = enabled ? style.text : mid(style.text, style.background);
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);

        if (focused) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
    }

    void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text, bool checked,
                           bool hovered, bool pressed, bool focused, bool enabled) const override {
        auto const &style = radio;
        auto fm = painter.font_metrics(style.font_size);
        auto r = style.box_size / 2.0f;
        auto center = Point{rect.x + r, rect.height / 2.0f};
        auto text_x = rect.x + style.box_size + style.spacing;
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto border = focused ? style.border_focused : style.border;

        painter.fill_circle(center, r, style.background);
        painter.draw_circle(center, r, border, style.border_width);

        if (style.beveled) {
            painter.draw_circle(center, r - 1.0f, style.shadow, 1.0f);
        }
        if (checked) {
            painter.fill_circle(center, r * 0.45f, style.indicator);
        }
        auto text_c = enabled ? style.text : mid(style.text, style.background);
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);
        if (focused) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
    }

    void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                         std::string_view placeholder, int cursor_pos, int selection_start,
                         int selection_end, bool focused, bool enabled, bool password_mode,
                         float scroll_offset, std::optional<Color> background) const override {
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

        auto text_c = enabled ? style.text : mid(style.text, style.background);

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

        if (focused && cursor_pos >= 0) {
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

        if (horizontal) {
            auto groove_y = rect.y + (rect.height - style.groove_thickness) / 2.0f;
            groove_rect = {rect.x, groove_y, rect.width, style.groove_thickness};
            auto handle_x = rect.x + rect.width * std::clamp(value, 0.0f, 1.0f);
            handle_rect = {handle_x - style.handle_size / 2.0f,
                           rect.y + (rect.height - style.handle_size) / 2.0f, style.handle_size,
                           style.handle_size};
        } else {
            auto groove_x = rect.x + (rect.width - style.groove_thickness) / 2.0f;
            groove_rect = {groove_x, rect.y, style.groove_thickness, rect.height};
            auto handle_y = rect.y + rect.height * std::clamp(value, 0.0f, 1.0f);
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
                  bool hovered, bool enabled, bool has_close, bool hovered_close) const override {
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

        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);

        if (has_close) {
            auto close_btn_size = 14.0f;
            auto close_gap = 6.0f;
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

        if (selected) {
            bg = style.selected_bg;
        } else if (hovered) {
            bg = style.hovered_bg;
        } else if (alternate) {
            bg = style.alternate_bg;
        } else {
            bg = style.background;
        }

        painter.fill_rect(rect, bg);

        auto fm = painter.font_metrics(style.font_size);
        auto text_x = rect.x + style.item_padding_h;
        if (icon) {
            text_x += static_cast<float>(icon->width) + style.item_padding;
        }
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_c = selected ? style.selected_text : style.text;
        painter.draw_text(text, {text_x, baseline_y}, text_c, style.font_size);
    }

    void draw_list_background(Painter &painter, Rect const &rect) const override {
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
    }

    void draw_table_background(Painter &painter, Rect const &rect) const override {
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

        if (focused && !open) {
            painter.draw_focus_ring(rect, style.corner_radius);
        }
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
            auto border_c = palette_.window.darken(0.15f);
            painter.draw_line({rect.x, rect.y + rect.height - 1.0f},
                              {rect.x + rect.width, rect.y + rect.height - 1.0f}, border_c, 1.0f);
        }
    }

    Size measure_label(std::string_view text, float font_size) const override {
        auto fm = Painter::measure_font_metrics(font_size);
        auto w = Painter::measure_text(text, font_size).width;
        return {w, fm.height + 4.0f};
    }

    Color error_color() const override { return palette_.error.lighten(0.3f); }

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

  protected:
    Palette palette_;
};

// ── Specific Themes ──────────────────────────────────────────────────────────

class MacOSTheme : public BaseTheme {
  public:
    using BaseTheme::BaseTheme;
};

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
    case ThemeStyle::MacOS:
        t = std::make_unique<MacOSTheme>(palette);
        break;
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
