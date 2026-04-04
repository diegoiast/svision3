// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme.hpp"
#include "toolkit/button_state.hpp"
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
        if (palette.fonts.size == 0 && palette.fonts.size > 0) {
            palette.fonts.size = palette.fonts.size;
        }

        // Initialize backward compatibility members
        name = "Base";
        style = ThemeStyle::Material;
        layout.margins = {8, 8, 8, 8};
        layout.spacing = 8.0f;
    }

    void draw_button(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                     ButtonState state, bool focused, bool enabled, bool flat,
                     std::optional<Color> background) const override {

        auto hovered = state == ButtonState::Hovered || state == ButtonState::ClickedInside;
        auto pressed = state == ButtonState::ClickedInside;
        auto border_c = (focused || hovered || pressed) ? palette.accent : palette.border;
        auto text_c = enabled ? palette.text : palette.text_disabled;
        auto text_offset = (palette.beveled && pressed && enabled) ? 1.0f : 0.0f;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_w = painter.text_size(text, palette.fonts.size).width;
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
        auto show_full_frame = !flat || hovered || pressed;

        auto defaultBg = palette.base;
        if (flat && state != ButtonState::Hovered) {
            defaultBg = palette.window;
        }
        auto bg = background.value_or(defaultBg);
        if (enabled && !background) {
            if (pressed && palette.background_pressed) {
                bg = *palette.background_pressed;
            } else if (focused) {
                // FIXME: do we need a focused color for Buttons?
                bg = palette.base;
            } else if (hovered && palette.background_hovered) {
                bg = *palette.background_hovered;
            }
        }
        if (show_full_frame) {
            painter.draw_filled_frame(rect, bg, border_c, palette, pressed && enabled);
        } else if (palette.corner_radius > 0.0f) {
            painter.fill_rounded_rect(rect, bg, palette.corner_radius);
        } else {
            painter.fill_rect(rect, bg);
        }
        if (icon) {
            auto icon_x = (rect.width - total_w) / 2.0f + text_offset;
            auto icon_y = (rect.height - icon_h) / 2.0f;
            painter.draw_image(*icon, Point{icon_x, icon_y});
        }
        painter.draw_text(text, text_pos, text_c, palette.fonts.size);
    }

    void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text,
                       CheckState check_state, ButtonState button_state, bool focused,
                       bool enabled) const override {
        auto const &style = checkbox;
        auto hovered =
            button_state == ButtonState::Hovered || button_state == ButtonState::ClickedInside;
        auto pressed = button_state == ButtonState::ClickedInside;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto box = style.box_size;
        auto box_y = (rect.height - box) / 2.0f;
        auto box_rect = Rect{rect.x, box_y, box, box};
        auto border = focused ? palette.accent : palette.border;
        auto bg = palette.base;
        if (enabled) {
            if (pressed) {
                bg = palette.alternate;
            } else if (button_state == ButtonState::ClickedOutside) {
                bg = palette.base;
            } else if (hovered) {
                bg = palette.alternate;
            }
        }

        painter.draw_filled_frame(box_rect, bg, border, palette, pressed && enabled);

        if (check_state == CheckState::Checked) {
            auto cx = box_rect.x + box * 0.22f;
            auto cy = box_rect.y + box * 0.5f;
            auto lw = std::max(1.5f, box * 0.14f);
            painter.draw_line({cx, cy}, {cx + box * 0.18f, cy + box * 0.2f}, palette.text, lw);
            painter.draw_line({cx + box * 0.18f, cy + box * 0.2f},
                              {cx + box * 0.55f, cy - box * 0.25f}, palette.text, lw);
        } else if (check_state == CheckState::Partial) {
            auto gap = box * 0.25f;
            auto inner = box_rect.inset(gap);
            painter.fill_rect(inner, palette.text_disabled);
        }

        auto text_x = rect.x + box + style.spacing;
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_c = enabled ? palette.text : palette.text_disabled;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);
    }

    void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text, bool checked,
                           ButtonState button_state, bool focused, bool enabled) const override {
        auto const &style = radio;
        auto hovered =
            button_state == ButtonState::Hovered || button_state == ButtonState::ClickedInside;
        auto pressed = button_state == ButtonState::ClickedInside;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto r = style.box_size / 2.0f;
        auto center = Point{rect.x + r, rect.height / 2.0f};
        auto text_x = rect.x + style.box_size + style.spacing;
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto border = focused ? palette.accent : palette.border;
        auto bg = palette.base;
        if (enabled) {
            if (pressed) {
                bg = palette.alternate;
            } else if (button_state == ButtonState::ClickedOutside) {
                bg = palette.base;
            } else if (hovered) {
                bg = palette.alternate;
            }
        }

        painter.fill_circle(center, r, bg);
        painter.draw_circle(center, r, border, palette.border_width);
        if (palette.beveled) {
            painter.draw_circle(center, r - 1.0f, palette.shadow, 1.0f);
        }
        if (checked) {
            painter.fill_circle(center, r * 0.45f, palette.text);
        }
        auto text_c = enabled ? palette.text : palette.text_disabled;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);
    }

    void draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                         std::string_view placeholder, int cursor_pos, int selection_start,
                         int selection_end, bool focused, bool enabled, bool password_mode,
                         float scroll_offset, std::optional<Color> background,
                         bool cursor_visible) const override {
        auto const &style = line_input;
        auto border = focused ? palette.accent : palette.border;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto content_x = style.padding.left;
        auto content_w = rect.width - style.padding.left - style.padding.right;
        auto tx = rect.x + content_x - scroll_offset;

        painter.draw_filled_frame(rect, palette.base, border, palette, true);

        auto clip_rect = Rect{rect.x + content_x, rect.y, content_w, rect.height};
        painter.push_clip(clip_rect);

        auto text_c = enabled ? palette.text : palette.text_disabled;
        if (selection_start >= 0 && selection_end > selection_start) {
            auto before_s = text.substr(0, selection_start);
            auto before_e = text.substr(0, selection_end);
            auto sx = tx + painter.text_size(before_s, palette.fonts.size).width;
            auto ex = tx + painter.text_size(before_e, palette.fonts.size).width;
            auto hy = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
            painter.fill_rect({sx, hy, ex - sx, fm.height + 2.0f}, sel_bg);
        }

        if (text.empty() && !focused) {
            painter.draw_text(placeholder, {tx, baseline_y}, palette.placeholder,
                              palette.fonts.size);
        } else if (!text.empty()) {
            if (password_mode) {
                auto dot_radius = palette.fonts.size * 0.25f;
                auto char_w = painter.text_size("8", palette.fonts.size).width;
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
                painter.draw_text(text, {tx, baseline_y}, text_c, palette.fonts.size);
            }
        }

        if (focused && cursor_pos >= 0 && cursor_visible) {
            auto before = text.substr(0, cursor_pos);
            auto cx = tx;
            if (!before.empty()) {
                cx += painter.text_size(before, palette.fonts.size).width;
            }
            auto cy_top = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto cy_bot = cy_top + fm.height + 2.0f;
            painter.draw_line({cx, cy_top}, {cx, cy_bot}, palette.text, 1.5f);
        }
        painter.pop_clip();
    }

    void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title, bool hovered,
                           bool active, bool show_mnemonics, int mnemonic_index) const override {
        auto const &style = menubar;       
        auto padding = style.padding;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_c = palette.text;

        if (hovered || active) {
            // FIXME: do we want hovered colors?
            // auto bg = style.background_hovered.value_or(style.background.darken(0.1f));
            auto bg = palette.highlight;
            painter.fill_rect(rect, bg);
            text_c = palette.highlighted_text;
        }

        auto baseline = (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(title, {rect.x + padding.left, baseline}, text_c, palette.fonts.size);

        if (show_mnemonics && mnemonic_index >= 0) {
            auto before = title.substr(0, mnemonic_index);
            auto ch = std::string(1, title[mnemonic_index]);
            auto before_w =
                before.empty() ? 0.0f : painter.text_size(before, palette.fonts.size).width;
            auto ch_w = painter.text_size(ch, palette.fonts.size).width;
            auto ul_y = baseline + fm.descent * 0.4f;

            painter.draw_line({rect.x + padding.left + before_w, ul_y},
                              {rect.x + padding.left + before_w + ch_w, ul_y}, text_c, 1.0f);
        }
    }

    void draw_menubar_background(Painter &painter, Rect const &rect) const override {
        // FIXME: do we want a different background for menubar?
        painter.fill_rect(rect, palette.window);
        auto border_c = palette.window;
        painter.draw_line({rect.x, rect.height - 1.0f}, {rect.x + rect.width, rect.height - 1.0f},
                          border_c, 1.0f);
    }

    void draw_menu_background(Painter &painter, Rect const &rect) const override {
        auto const &style = combobox;
        auto shadow = Color::rgba(0, 0, 0, 0.12f);
        painter.fill_rounded_rect({rect.x + 1, rect.y + 1, rect.width, rect.height}, shadow,
                                  palette.corner_radius);
        painter.fill_rounded_rect(rect, palette.base, palette.corner_radius);
        painter.draw_rounded_rect(rect, palette.border, palette.corner_radius,
                                  palette.border_width);
    }

    void draw_menu_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        std::string_view shortcut, bool hovered, bool enabled, bool checkable,
                        bool checked) const override {
        auto const &style = combobox;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto baseline = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = palette.text;

        if (hovered && enabled) {
            // FIXME: highlight hovered is it needed?
            painter.fill_rounded_rect(rect, palette.highlight, palette.corner_radius * 0.5f);
        }

        if (checkable && checked) {
            auto check_rect = Rect{rect.x + 4, rect.y + (rect.height - 12) / 2, 12, 12};
            painter.fill_rounded_rect(check_rect, palette.border, 2.0f);
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
        painter.draw_text(text, {text_x, baseline}, text_col, palette.fonts.size);

        if (!shortcut.empty()) {
            auto shortcut_w = painter.text_size(shortcut, palette.fonts.size).width;
            auto shortcut_x = rect.x + rect.width - style.padding.right - shortcut_w - 10.0f;
            painter.draw_text(shortcut, {shortcut_x, baseline}, text_col, palette.fonts.size);
        }
    }

    void draw_menu_separator(Painter &painter, Rect const &rect) const override {
        auto const &style = combobox;
        auto mid_y = rect.y + rect.height / 2.0f;
        auto sep_col = palette.border;
        sep_col.a *= 0.5f;
        painter.draw_line({rect.x + 8, mid_y}, {rect.x + rect.width - 8, mid_y}, sep_col, 0.5f);
    }

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           bool enabled) const override {
        auto bg = palette.window;
        auto fill = palette.accent;

        auto inner = rect.inset(palette.border_width);
        auto fill_w = inner.width * std::clamp(progress, 0.0f, 1.0f);
        auto fill_rect = Rect{inner.x, inner.y, fill_w, inner.height};

        painter.draw_filled_frame(rect, bg, palette.border, palette, true);
        painter.fill_rect(fill_rect, fill);
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

        painter.fill_rounded_rect(groove_rect, palette.border, style.groove_thickness / 2.0f);

        // FIXME: we need hover color?
        auto bg = pressed ? palette.window : palette.base;
        auto border = palette.border;
        if (hovered && palette.background_hovered) {
            bg = *palette.background_hovered;
        }
        if (pressed && palette.background_pressed) {
            bg = *palette.background_pressed;
        }
        if (hovered || pressed) {
            border = palette.accent;
        }
        painter.fill_rounded_rect(handle_rect, bg, style.handle_size / 4.0f);
        painter.draw_rounded_rect(handle_rect, palette.border, style.handle_size / 4.0f, 1.0f);
    }

    void draw_tab_bar_background(Painter &painter, Rect const &rect) const override {
        painter.fill_rect(rect, palette.window);
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto const &style = tab_widget;
        auto bg = palette.window;
        if (active) {
            bg = palette.base;
        } else {
            if (hovered && palette.background_hovered) {
                bg = *palette.background_hovered;
            }
        }

        if (active) {
            painter.fill_rounded_rect(rect, bg, palette.tab_radius);
        } else {
            painter.fill_rect(rect, bg);
        }

        auto vertical =
            (orientation == TabOrientation::West || orientation == TabOrientation::East);
        auto text_orientation = Painter::TextOrientation::Horizontal;
        if (orientation == TabOrientation::West) {
            text_orientation = Painter::TextOrientation::VerticalCW;
        } else if (orientation == TabOrientation::East) {
            text_orientation = Painter::TextOrientation::VerticalCCW;
        }

        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_w = painter.text_size(text, palette.fonts.size).width;
        auto text_x = 0.0f, baseline_y = 0.0f;
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

        auto text_c = /*active ? palette.highlighted_text : */ palette.text;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size,
                          FontFamily::System, text_orientation);

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

        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_x = rect.x + style.item_padding_h;
        if (icon) {
            auto icon_y = rect.y + (rect.height - static_cast<float>(icon->height)) / 2.0f;
            painter.draw_image(*icon, {text_x, icon_y});
            text_x += static_cast<float>(icon->width) + style.item_padding;
        }
        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_c = selected ? palette.highlighted_text : palette.text;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);
    }

    void draw_list_background(Painter &painter, Rect const &rect, bool focused) const override {

        if (palette.beveled) {
            painter.draw_filled_frame(rect, palette.base, palette.border, palette, true);
        } else {
            painter.fill_rounded_rect(rect, palette.base, palette.corner_radius);
            if (palette.border_width > 0) {
                painter.draw_rounded_rect(rect, palette.border, palette.corner_radius,
                                          palette.border_width);
            }
        }
        if (focused) {
            painter.draw_focus_ring(rect, palette.corner_radius);
        }
    }

    void draw_table_background(Painter &painter, Rect const &rect, bool focused) const override {
        auto const &style = table_view;
        if (palette.beveled) {
            painter.draw_filled_frame(rect, palette.base, palette.border, palette, true);
        } else {
            painter.fill_rounded_rect(rect, palette.base, palette.corner_radius);
            if (palette.border_width > 0) {
                painter.draw_rounded_rect(rect, palette.border, palette.corner_radius,
                                          palette.border_width);
            }
        }
        if (focused) {
            painter.draw_focus_ring(rect, palette.corner_radius);
        }
    }

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override {
        auto const &style = tree_view;
        auto fm = painter.font_metrics(palette.fonts.size);

        auto x_offset = style.item_padding_h + depth * style.indent;

        if (has_children) {
            auto arrow_x = x_offset + 4;
            auto arrow_y = rect.y + rect.height / 2;
            auto arrow_size = 8.0f;

            if (expanded) {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size / 2, arrow_y + arrow_size / 2},
                                      palette.text);
            } else {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x, arrow_y + arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y}, palette.text);
            }
        }

        x_offset += style.indent + 4.0f;

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? palette.highlighted_text : palette.text;
        painter.draw_text(text, {x_offset, text_y}, text_col, palette.fonts.size);
    }

    void draw_tree_background(Painter &painter, Rect const &rect, bool focused) const override {
        if (palette.beveled) {
            painter.draw_filled_frame(rect, palette.window, palette.border, palette, true);
        } else {
            painter.fill_rounded_rect(rect, palette.window, palette.corner_radius);
            if (palette.border_width > 0) {
                painter.draw_rounded_rect(rect, palette.border, palette.corner_radius,
                                          palette.border_width);
            }
        }
        if (focused) {
            painter.draw_focus_ring(rect, palette.corner_radius);
        }
    }

    void draw_combobox(Painter &painter, Rect const &rect, std::string_view text, bool focused,
                       bool open) const override {
        auto const &style = combobox;
        auto border = focused ? palette.accent : palette.border;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;

        painter.draw_filled_frame(rect, palette.window, border, palette, true);

        if (!text.empty()) {
            auto clip_w = rect.width - style.padding.left - style.padding.right - 16.0f;
            painter.push_clip({style.padding.left, 0, clip_w, rect.height});
            painter.draw_text(text, {style.padding.left, baseline_y}, palette.text,
                              palette.fonts.size);
            painter.pop_clip();
        }

        auto arrow_x = rect.width - style.padding.right - 8.0f;
        auto arrow_y = rect.height / 2.0f;
        auto aw = 4.0f;
        painter.draw_line({arrow_x - aw, arrow_y - 2.0f}, {arrow_x, arrow_y + 2.0f}, palette.text,
                          1.5f);
        painter.draw_line({arrow_x, arrow_y + 2.0f}, {arrow_x + aw, arrow_y - 2.0f}, palette.text,
                          1.5f);
    }

    void draw_combobox_item(Painter &painter, Rect const &rect, std::string_view text,
                            bool hovered) const override {
        auto const &style = combobox;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto baseline = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto tc = hovered ? palette.highlighted_text : palette.text;

        if (hovered) {
            painter.fill_rect(rect, palette.highlight);
        }

        painter.draw_text(text, {rect.x + style.padding.left, baseline}, tc, palette.fonts.size);
    }

    void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const override {
        if (palette.corner_radius > 0.0f) {
            painter.fill_rounded_rect(rect, palette.tooltip, palette.corner_radius);
            painter.draw_rounded_rect(rect, palette.border, palette.corner_radius,
                                      palette.border_width);
        } else {
            painter.fill_rect(rect, palette.tooltip);
            painter.draw_rect(rect, palette.border, palette.border_width);
        }

        // FIXME: padding is hardcoded for tooltips
        auto padding = 5;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_x = rect.x + padding;
        auto baseline_y = rect.y + padding + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, palette.text, palette.fonts.size);
    }

    void draw_toolbar(Painter &painter, Rect const &rect) const override {
        if (palette.beveled) {
            painter.draw_line({rect.x, rect.y}, {rect.x + rect.width, rect.y}, palette.highlight,
                              1.0f);
            painter.draw_line({rect.x, rect.y + rect.height - 1.0f},
                              {rect.x + rect.width, rect.y + rect.height - 1.0f}, palette.shadow,
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
        auto border = focused ? palette.accent : palette.border;

        painter.draw_filled_frame(field_rect, palette.base, border, palette, true);

        auto content_x = field_rect.x + style.padding.left;
        auto content_w = field_rect.width - style.padding.left - style.padding.right;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto clip_rect = Rect{content_x, rect.y, content_w, rect.height};
        painter.push_clip(clip_rect);

        if (selection_start >= 0 && selection_end > selection_start) {
            auto before_s = text.substr(0, selection_start);
            auto before_e = text.substr(0, selection_end);
            auto sx = content_x + painter.text_size(before_s, palette.fonts.size).width;
            auto ex = content_x + painter.text_size(before_e, palette.fonts.size).width;
            auto hy = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
            painter.fill_rect({sx, hy, ex - sx, fm.height + 2.0f}, sel_bg);
        }

        auto text_c = enabled ? palette.text : palette.text_disabled;
        painter.draw_text(text, {content_x, baseline_y}, text_c, palette.fonts.size);

        if (focused && cursor_pos >= 0 && cursor_visible) {
            auto before = text.substr(0, cursor_pos);
            auto cx = content_x;
            if (!before.empty()) {
                cx += painter.text_size(before, palette.fonts.size).width;
            }
            auto cy_top = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
            auto cy_bot = cy_top + fm.height + 2.0f;
            painter.draw_line({cx, cy_top}, {cx, cy_bot}, palette.text, 1.5f);
        }

        painter.pop_clip();

        auto btn_w = bw;
        auto up_rect = Rect{rect.x + rect.width - btn_w, rect.y, btn_w, rect.height / 2.0f};
        auto down_rect = Rect{rect.x + rect.width - btn_w, rect.y + rect.height / 2.0f, btn_w,
                              rect.height / 2.0f};

        auto draw_spinbox_button = [&](Rect const &r, bool hovered, bool pressed) {
            auto b_bg = palette.window;
            if (pressed && palette.background_pressed) {
                b_bg = *palette.background_pressed;
            } else if (hovered && palette.background_hovered) {
                b_bg = *palette.background_hovered;
            }
            painter.draw_filled_frame(r, b_bg, border, palette, false);

            auto cx = r.x + r.width / 2.0f;
            auto cy = r.y + r.height / 2.0f;
            auto arrow_sz = 3.5f;
            auto tc = palette.text;

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
        auto fm = painter.font_metrics(palette.fonts.size, FontFamily::Monospace);
        auto bg = palette.base;
        auto border = focused ? palette.accent : palette.border;
        auto text_c = enabled ? palette.text : palette.text_disabled;

        painter.draw_filled_frame(rect, bg, border, palette, true);
        painter.push_clip(rect);

        auto last = std::min(static_cast<int>(lines.size()) - 1,
                             first_visible_line + static_cast<int>(rect.height / line_height));

        auto gutter_rect = Rect{rect.x, rect.y, gutter_width, rect.height};
        auto gutter_bg = palette.base;
        painter.fill_rect(gutter_rect, gutter_bg);

        for (auto i = first_visible_line; i <= last; i++) {
            auto y = rect.y + line_height * static_cast<float>(i - first_visible_line);
            auto baseline = y + (line_height - fm.height) / 2.0f + fm.ascent;
            auto num = std::to_string(i + 1);
            auto nw = Painter::measure_text(num, palette.fonts.size, FontFamily::Monospace).width;
            painter.draw_text(num, {rect.x + gutter_width - nw - 8.0f, baseline},
                              palette.placeholder, palette.fonts.size, FontFamily::Monospace);
        }

        auto area = Rect{rect.x + gutter_width, rect.y, rect.width - gutter_width, rect.height};
        auto tx0 = rect.x + gutter_width - scroll_x;
        // FIXME - hardcoded color
        auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
        auto has_sel = selection_start_line >= 0 && selection_end_line >= 0 &&
                       (selection_start_line < selection_end_line ||
                        (selection_start_line == selection_end_line &&
                         selection_start_col < selection_end_col));

        painter.push_clip(area);
        for (auto i = first_visible_line; i <= last; i++) {
            auto y = rect.y + line_height * static_cast<float>(i - first_visible_line);
            auto baseline = y + (line_height - fm.height) / 2.0f + fm.ascent;

            if (has_sel) {
                auto line_start_col = 0;
                auto line_end_col = static_cast<int>(lines[i].size());
                auto sel_start = (i == selection_start_line) ? selection_start_col : line_start_col;
                auto sel_end = (i == selection_end_line) ? selection_end_col : line_end_col;

                if (sel_start < sel_end) {
                    auto sx = tx0 + (sel_start > 0 ? Painter::measure_text(
                                                         lines[i].substr(0, sel_start),
                                                         palette.fonts.size, FontFamily::Monospace)
                                                         .width
                                                   : 0.0f);
                    auto ex = tx0 + Painter::measure_text(lines[i].substr(0, sel_end),
                                                          palette.fonts.size, FontFamily::Monospace)
                                        .width;
                    if (i != selection_end_line) {
                        ex += palette.fonts.size * 0.4f;
                    }
                    painter.fill_rect({sx, y, ex - sx, line_height}, sel_bg);
                }
            }

            painter.draw_text(lines[i], {tx0, baseline}, text_c, palette.fonts.size,
                              FontFamily::Monospace);
        }

        if (focused) {
            // FIXME: cursor should not be handled here. Just pass "cursor_blink".
            auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            if ((ms / 500) % 2 == 0) {
                auto cy =
                    rect.y + line_height * static_cast<float>(cursor_line - first_visible_line);
                auto cx = tx0;
                if (cursor_col > 0 && cursor_line < static_cast<int>(lines.size())) {
                    cx += Painter::measure_text(lines[cursor_line].substr(0, cursor_col),
                                                palette.fonts.size, FontFamily::Monospace)
                              .width;
                }
                painter.draw_line({cx, cy}, {cx, cy + line_height}, palette.text, 1.5f);
            }
        }

        painter.pop_clip();

        auto content_h = line_height * static_cast<float>(lines.size());
        if (content_h > rect.height) {
            auto bar_h = std::max(20.0f, rect.height * (rect.height / content_h));
            auto bar_y = (scroll_y / content_h) * rect.height;
            auto sb = Rect{rect.x + rect.width - 6.0f, rect.y + bar_y, 4.0f, bar_h};
            // FIXME: whats is this 2.0f?
            painter.fill_rounded_rect(sb, palette.text, 2.0f);
        }

        painter.pop_clip();
    }

    void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const override {
        Color ring = palette.border;
        ring.a = 0.5f;
        auto lw = 2.0f;
        auto inset = lw / 2.0f + 0.5f;
        auto r = rect.inset(inset);
        auto dash_len = 2.0f;
        auto gap_len = 2.0f;

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

        auto cr = std::max(0.0f, corner_radius - inset);
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

    Size measure_button(std::string_view text, Icon const &icon) const override {
        auto text_w = Painter::measure_text(text, palette.fonts.size).width;
        auto icon_w = 0.0f;
        if (icon) {
            icon_w = static_cast<float>(icon->width);
        }
        auto total_w = text_w + (icon ? icon_w + 4.0f : 0.0f);
        auto w = total_w + button.padding.left + button.padding.right;
        auto h = palette.fonts.size + button.padding.top + button.padding.bottom;
        return {w, h};
    }

    Size measure_checkbox(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, palette.fonts.size).width;
        auto w = checkbox.box_size + checkbox.spacing + text_w;
        auto h = std::max(checkbox.box_size, palette.fonts.size);
        return Size{w, h};
    }

    Size measure_radio_button(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, palette.fonts.size).width;
        auto w = radio.box_size + radio.spacing + text_w;
        auto h = std::max(radio.box_size, palette.fonts.size);
        return Size{w, h};
    }

    Size measure_menubar_item(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, palette.fonts.size).width;
        return {text_w + menubar.padding.left + menubar.padding.right, 0};
    }

    Size measure_menu_item(std::string_view text, Icon const &icon,
                           std::string_view shortcut) const override {
        auto w = menu.item_padding * 2;
        if (icon) {
            w += static_cast<float>(icon->width) + menu.item_padding;
        }
        w += Painter::measure_text(text, palette.fonts.size).width;
        if (!shortcut.empty()) {
            w += menu.item_padding + Painter::measure_text(shortcut, palette.fonts.size).width;
        }
        auto h = palette.fonts.size + menu.item_padding * 2;
        return {w, h};
    }

    float menu_separator_height() const override { return 8.0f; }
    Size measure_tab(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, palette.fonts.size).width;
        auto w = text_w + tab_widget.tab_padding_h * 2;
        auto h = palette.fonts.size + tab_widget.tab_padding_v * 2;
        return {w, h};
    }
    float list_item_height() const override { return 24.0f; }
    Size measure_tooltip(std::string_view text) const override {
        auto text_w = Painter::measure_text(text, palette.fonts.size).width;

        // FIXME: padding is hardcoded for tabs
        auto padding = 0.5f;
        auto w = text_w + padding * 2;
        auto h = palette.fonts.size + padding * 2;
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
        auto fm = painter.font_metrics(palette.fonts.size);

        if (hovered || active) {
            Color bg = palette.highlight;
            auto hover_rect = rect.inset(2.0f);
            painter.fill_rounded_rect(hover_rect, bg, palette.corner_radius);
        }

        auto baseline = (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_c = palette.text;
        if (hovered || active) {
            text_c = palette.highlighted_text;
        }
        painter.draw_text(title, {rect.x + padding.left, baseline}, text_c, palette.fonts.size);
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
        auto fm = painter.font_metrics(palette.fonts.size);
        auto indent = style.indent;

        auto x_offset = style.item_padding_h + depth * indent;

        if (has_children) {
            auto arrow_x = x_offset + 4;
            auto arrow_y = rect.y + rect.height / 2;
            auto arrow_size = 8.0f;

            if (expanded) {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y},
                                      {arrow_x, arrow_y + arrow_size / 2}, palette.text);
            } else {
                painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size, arrow_y - arrow_size / 2},
                                      {arrow_x + arrow_size / 2, arrow_y + arrow_size / 2},
                                      palette.text);
            }
            x_offset += indent;
        }

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? palette.accent : palette.text;
        painter.draw_text(text, {x_offset, text_y}, text_col, palette.fonts.size);
    }
};

class Win95Theme : public BaseTheme {
  public:
    explicit Win95Theme(Palette p) : BaseTheme(std::move(p)) {
        name = "Windows 95";
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
        auto ring = palette.border;
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
        auto fm = painter.font_metrics(palette.fonts.size);
        auto x_offset = style.item_padding_h + depth * style.indent;

        if (has_children) {
            auto icon_x = x_offset;
            auto box_size = cb_style.box_size;
            auto box_rect =
                Rect{icon_x, rect.y + (rect.height - box_size) / 2.0f, box_size, box_size};

            painter.draw_filled_frame(box_rect, palette.base, palette.border, palette, false);

            auto expand_collapse_char = expanded ? "-" : "+";
            auto char_w = painter.text_size(expand_collapse_char, palette.fonts.size).width;
            auto char_x = icon_x + (box_size - char_w) / 2.0f;
            auto char_y = box_rect.y + (box_size - fm.height) / 2.0f + fm.ascent;

            painter.draw_text(expand_collapse_char, Point{char_x, char_y}, palette.text,
                              palette.fonts.size);
        }

        x_offset += style.indent + 4.0f;

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? palette.highlighted_text : palette.text;
        painter.draw_text(text, Point{x_offset, text_y}, text_col, palette.fonts.size);
    }

    void draw_tree_background(Painter &painter, Rect const &rect, bool focused) const override {
        auto const &style = tree_view;
        if (palette.beveled) {
            painter.draw_filled_frame(rect, palette.window, palette.border, palette, true);
        } else {
            painter.fill_rounded_rect(rect, palette.window, palette.corner_radius);
            if (palette.border_width > 0) {
                painter.draw_rounded_rect(rect, palette.border, palette.corner_radius,
                                          palette.border_width);
            }
        }
    }

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           bool enabled) const override {
        auto bg = enabled ? palette.window : palette.window.darken(0.1f);
        auto fill_c = enabled ? palette.accent : palette.accent.darken(0.2f);
        const auto chunk_width = 8.0f;
        const auto chunk_gap = 2.0f;

        painter.draw_filled_frame(rect, bg, palette.border, palette, true);

        auto inner = rect.inset(palette.border_width);
        auto fill_w = inner.width * std::clamp(progress, 0.0f, 1.0f);
        auto fill_rect = Rect{inner.x, inner.y, fill_w, inner.height};

        auto chunk_count = static_cast<int>(inner.width / (chunk_width + chunk_gap));
        for (int i = 0; i < chunk_count; ++i) {
            auto cx = inner.x + i * (chunk_width + chunk_gap);
            if (cx + chunk_width > inner.x + fill_w) {
                break;
            }
            painter.fill_rect({cx, inner.y, chunk_width, inner.height}, fill_c);
        }
    }
};

class MaterialTheme : public BaseTheme {
  public:
    explicit MaterialTheme(Palette p) : BaseTheme(std::move(p)) {
        auto is_dark = palette.window.luma() < 0.5f;
        name = "Material";
        //button.background_hovered =
        //    is_dark ? palette.window.lighten(0.08f) : palette.window.darken(0.04f);
        //button.background_pressed =
        //    is_dark ? palette.window.lighten(0.15f) : palette.window.darken(0.10f);
        button.padding = {10, 24, 10, 24};
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
        auto bg = palette.window;
        if (active) {
            bg = palette.base;
            if (palette.background_pressed) {
                bg = *palette.background_pressed;
            }
        } else {
            if (hovered && palette.background_hovered) {
                bg = *palette.background_hovered;
            }
        }

        auto text_c = active ? palette.highlighted_text : palette.text;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_w = painter.text_size(text, palette.fonts.size).width;
        auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
        auto left_space = style.tab_padding_h;
        auto text_area_w = rect.width - left_space - right_space;
        auto text_x = rect.x + left_space + (text_area_w - text_w) / 2.0f;
        if (text_x < rect.x + left_space) {
            text_x = rect.x + left_space;
        }

        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);

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
        button.padding = {8, 20, 8, 20};

        slider.handle_size = 22.0f;
        slider.groove_thickness = 6.0f;

        focus_ring_margin = 2.0f;
        focus_ring_corner_radius = 2.0f;
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        auto const &style = tab_widget;
        // FIXME: we do not support hover colors. Should we use the button colors?
        auto bg = palette.window;
        if (active) {
            if (palette.background_pressed) {
                bg = *palette.background_pressed;
            }
        } else {
            if (hovered && palette.background_hovered) {
                bg = *palette.background_hovered;
            }
        }
        auto text_c = active ? palette.highlighted_text : palette.text;
        auto tab_rect = rect.inset(2.0f);
        painter.fill_rounded_rect(tab_rect, bg, 6.0f);

        auto fm = painter.font_metrics(palette.fonts.size);
        auto text_w = painter.text_size(text, palette.fonts.size).width;
        auto right_space = has_close ? (style.tab_padding_h + 14.0f + 6.0f) : 0.0f;
        auto left_space = style.tab_padding_h;
        auto text_area_w = rect.width - left_space - right_space;
        auto text_x = rect.x + left_space + (text_area_w - text_w) / 2.0f;
        if (text_x < rect.x + left_space) {
            text_x = rect.x + left_space;
        }

        auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);

        if (has_close) {
            // FIXME: clsoe button size is hardcoded
            auto close_btn_size = 14.0f;
            auto close_x = rect.x + rect.width - style.tab_padding_h - close_btn_size - 2.0f;
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
        button.padding = {9, 18, 9, 18};
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
                {rect.x + palette.corner_radius, rect.y + rect.height - 2.0f},
                {rect.x + rect.width - palette.corner_radius, rect.y + rect.height - 2.0f}, line_c,
                1.0f);
        }
    }

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override {
        BaseTheme::draw_tab(painter, rect, text, active, hovered, enabled, orientation, has_close,
                            hovered_close);

        // FIXME: move indicators the base theme, with an indicator size/position in the palette
        if (active) {
            auto indicator = Rect{};
            auto lw = 4.0f;
            auto r2 = rect;

            r2.x += palette.tab_radius;
            r2.width -= palette.tab_radius * 2;

            if (orientation == TabOrientation::North) {
                indicator = {r2.x, 0, r2.width, lw};
            } else if (orientation == TabOrientation::South) {
                indicator = {r2.x, r2.y, r2.width, lw};
            } else if (orientation == TabOrientation::West) {
                indicator = {lw, r2.y, lw, r2.height};
            } else if (orientation == TabOrientation::East) {
                indicator = {r2.x, r2.y, lw, r2.height};
            }
            // FIXME- this is not ideal, as the marker should also have a rounded corners.
            painter.fill_rect(indicator, palette.accent);
        }
    }
    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override {
        auto const &style = tree_view;
        auto fm = painter.font_metrics(palette.fonts.size);
        auto indent = style.indent;

        auto x_offset = style.item_padding_h + depth * indent;

        if (has_children) {
            auto center_x = x_offset + indent / 2;
            auto arrow_y = rect.y + rect.height / 2;
            auto arrow_size = 8.0f;
            auto arrow_offset = arrow_size * 0.3f;

            if (expanded) {
                painter.draw_line({center_x - arrow_size / 2, arrow_y - arrow_offset},
                                  {center_x, arrow_y + arrow_size / 2}, palette.text, 1.5f);
                painter.draw_line({center_x, arrow_y + arrow_size / 2},
                                  {center_x + arrow_size / 2, arrow_y - arrow_offset}, palette.text,
                                  1.5f);
            } else {
                painter.draw_line({center_x - arrow_offset, arrow_y - arrow_size / 2},
                                  {center_x + arrow_offset, arrow_y}, palette.text, 1.5f);
                painter.draw_line({center_x - arrow_offset, arrow_y + arrow_size / 2},
                                  {center_x + arrow_offset, arrow_y}, palette.text, 1.5f);
            }
        }

        x_offset += indent + 4.0f;

        auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
        auto text_col = selected ? palette.highlighted_text : palette.text;
        painter.draw_text(text, {x_offset, text_y}, text_col, palette.fonts.size);
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
    auto macBlue = Color::from_argb(0xFF0A84FF);
    p.border_width = 0.5f;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFF2F2F7);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF9F9FB);
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFF8E8E93);
        p.placeholder = Color::from_argb(0xFFAEAEB2);
        p.highlight = macBlue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFD1D1D6);
        p.accent = macBlue;
        p.link = macBlue;
        p.shadow = Color::from_argb(0x33000000);
        p.dark_shadow = Color::from_argb(0x55000000);
        p.background_pressed = Color::from_argb(0xFFE5E5EA);
        p.background_hovered = Color::from_argb(0xFFEDEDF0);
        p.tooltip = Color::from_argb(0xF2F2F2F2);
        p.success = Color::from_argb(0xFF34C759);
        p.warning = Color::from_argb(0xFFFF9F0A);
        p.error = Color::from_argb(0xFFFF3B30);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF1E1E1E);
        p.base = Color::from_argb(0xFF2C2C2E);
        p.alternate = Color::from_argb(0xFF3A3A3C);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF8E8E93);
        p.placeholder = Color::from_argb(0xFF636366);
        p.highlight = macBlue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF3A3A3C);
        p.accent = macBlue;
        p.link = macBlue;
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::from_argb(0xE62C2C2E);
        p.background_pressed = Color::from_argb(0xFF3A3A3C);
        p.background_hovered = Color::from_argb(0xFF48484A);
        p.success = Color::from_argb(0xFF30D158);
        p.warning = Color::from_argb(0xFFFF9F0A);
        p.error = Color::from_argb(0xFFFF453A);
        break;
    }
}

static void palette_win11(Palette &p, ColorScheme scheme) {
    auto windows_blue = Color::from_argb(0xFF0078D4);

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFF3F3F3);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF9F9F9);
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFF6D6D6D);
        p.placeholder = Color::from_argb(0xFF8A8A8A);
        p.highlight = windows_blue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFDCDCDC);
        p.accent = windows_blue;
        p.link = Color::from_argb(0xFF0067C0);
        p.shadow = Color::from_argb(0x20000000);
        p.dark_shadow = Color::from_argb(0x40000000);
        p.tooltip = Color::from_argb(0xFFFFFFFF);
        p.background_pressed = Color::from_argb(0xFFE5E5E5);
        p.background_hovered = Color::from_argb(0xFFEDEDED);
        p.success = Color::from_argb(0xFF107C10);
        p.warning = Color::from_argb(0xFFFFB900);
        p.error = Color::from_argb(0xFFD13438);
        break;

    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF202020);
        p.base = Color::from_argb(0xFF2B2B2B);
        p.alternate = Color::from_argb(0xFF333333);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF8A8A8A);
        p.placeholder = Color::from_argb(0xFF6D6D6D);
        p.highlight = windows_blue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF3C3C3C);
        p.accent = windows_blue;
        p.link = Color::from_argb(0xFF4CC2FF);
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::from_argb(0xFF2B2B2B);
        p.background_pressed = Color::from_argb(0xFF3A3A3A);
        p.background_hovered = Color::from_argb(0xFF444444);
        p.success = Color::from_argb(0xFF6CCB5F);
        p.warning = Color::from_argb(0xFFFFC83D);
        p.error = Color::from_argb(0xFFFF5F5F);
        break;
    }
}

static void palette_material(Palette &p, ColorScheme scheme) {
    // default Material 3 primary (Deep Purple)
    Color material_purple = Color::from_rgb(0x6750A4);
    p.corner_radius = 4.0f;
    p.tab_radius = 0.0f;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xFFFBFE);
        p.base = Color::from_rgb(0xFFFFFF);
        p.alternate = Color::from_rgb(0xF7F2FA);
        p.text = Color::from_rgb(0x1C1B1F);
        p.text_disabled = Color::from_rgb(0x9E9E9E);
        p.placeholder = Color::from_rgb(0x79747E);
        p.highlight = material_purple;
        p.highlighted_text = Color::from_rgb(0x000000);
        p.border = Color::from_rgb(0xE7E0EC);
        p.accent = material_purple;
        p.link = Color::from_rgb(0xFF2962FF);
        p.shadow = Color::from_rgb(0x000000);
        p.dark_shadow = Color::from_rgb(0x000000);
        p.background_pressed = Color::from_rgb(0xE8DEF8);
        p.background_hovered = Color::from_rgb(0xF3EDF7);
        p.tooltip = Color::from_rgb(0xE6E1E5);
        p.success = Color::from_rgb(0x2E7D32);
        p.warning = Color::from_rgb(0xF9A825);
        p.error = Color::from_rgb(0xB3261E);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_rgb(0x1C1B1F);
        p.base = Color::from_rgb(0x1C1B1F);
        p.alternate = Color::from_rgb(0x292529);
        p.text = Color::from_rgb(0xE6E1E5);
        p.text_disabled = Color::from_rgb(0x9E9E9E);
        p.placeholder = Color::from_rgb(0x79747E);
        p.highlight = material_purple;
        p.highlighted_text = Color::from_rgb(0xFFFFFF);
        p.border = Color::from_rgb(0x49454F);
        p.accent = material_purple;
        p.link = Color::from_rgb(0x82B1FF);
        p.shadow = Color::from_rgb(0x000000);
        p.dark_shadow = Color::from_argb(0x66000000);
        p.background_pressed = Color::from_rgb(0x3E3748);
        p.background_hovered = Color::from_rgb(0x4A4256);
        p.tooltip = Color::from_rgb(0x2B2B2B);
        p.success = Color::from_rgb(0x2E7D32);
        p.warning = Color::from_rgb(0xF9A825);
        p.error = Color::from_rgb(0xB3261E);
        break;
    }
}

static void palette_win95(Palette &p, ColorScheme scheme) {
    Color windows95_color = Color::from_argb(0xFF000080);
    p.beveled = true;
    p.progress_bar_height = 20;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFC0C0C0);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFC0C0C0);
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFF808080);
        p.placeholder = Color::from_argb(0xFF808080);
        p.highlight = windows95_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF808080);
        p.accent = windows95_color;
        p.link = windows95_color;
        p.shadow = Color::from_argb(0xFF404040);
        p.dark_shadow = Color::from_argb(0xFF000000);
        p.background_pressed = Color::from_argb(0xFFB0B0B0);
        p.background_hovered = Color::from_argb(0xFFB8B8B8);
        p.tooltip = Color::from_argb(0xFFFFFFE1);
        p.success = Color::from_argb(0xFF008000);
        p.warning = Color::from_argb(0xFFFF8000);
        p.error = Color::from_argb(0xFF800000);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF000000);
        p.base = Color::from_argb(0xFF202020);
        p.alternate = Color::from_argb(0xFF303030);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF808080);
        p.placeholder = Color::from_argb(0xFF808080);
        p.highlight = windows95_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF404040);
        p.accent = windows95_color;
        p.link = windows95_color;
        p.shadow = Color::from_argb(0xFF000000);
        p.dark_shadow = Color::from_argb(0xFF000000);
        p.background_pressed = windows95_color;
        p.background_hovered = Color::from_argb(0xFF303030);
        p.tooltip = Color::from_argb(0xFFFFFFE1);
        p.success = Color::from_argb(0xFF008000);
        p.warning = Color::from_argb(0xFFFF8000);
        p.error = Color::from_argb(0xFF800000);
        break;
    }
}

static void palette_plasma6(Palette &p, ColorScheme scheme) {
    // default Breeze accent blue
    Color plasma6_color = Color::from_argb(0xFF3DAEE9);
    p.corner_radius = 5.0f;
    p.tab_radius = 5.0f;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFeff0f1);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF7F7F7);
        p.text = Color::from_argb(0xFF2E3436);
        p.text_disabled = Color::from_argb(0xFF7F8C8D);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = plasma6_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFE0E0E0);
        p.accent = plasma6_color;
        p.link = plasma6_color;
        p.shadow = Color::from_argb(0x22000000);
        p.dark_shadow = Color::from_argb(0x44000000);
        p.background_pressed = Color::from_argb(0xFFE0E0E0);
        p.background_hovered = Color::from_argb(0xFFa3d4fa);
        p.tooltip = Color::from_argb(0xFFFDFDFD);
        p.success = Color::from_argb(0xFF27AE60);
        p.warning = Color::from_argb(0xFFF39C12);
        p.error = Color::from_argb(0xFFE74C3C);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF232629);
        p.base = Color::from_argb(0xFF1B1E20);
        p.alternate = Color::from_argb(0xFF31363B);
        p.text = Color::from_argb(0xFFEFF0F1);
        p.text_disabled = Color::from_argb(0xFF7F8C8D);
        p.placeholder = Color::from_argb(0xFFBDC3C7);
        p.highlight = plasma6_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF3B4045);
        p.accent = plasma6_color;
        p.link = plasma6_color;
        p.shadow = Color::from_argb(0x33000000);
        p.dark_shadow = Color::from_argb(0x55000000);
        p.background_pressed = Color::from_argb(0xFF2C3034);
        p.background_hovered = Color::from_argb(0xFF3B4045);
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.success = Color::from_argb(0xFF27AE60);
        p.warning = Color::from_argb(0xFFF39C12);
        p.error = Color::from_argb(0xFFE74C3C);
        break;
    }
}

static void palette_gnome(Palette &p, ColorScheme scheme) {
    p.corner_radius = 8.0f;

    auto adwaita_color = Color::from_argb(0xFF3465A4);

    switch (scheme) {
    case ColorScheme::Light:
        // default Adwaita blue accent

        p.window = Color::from_argb(0xFFFAFAFA);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF0F0F0);
        p.text = Color::from_argb(0xFF2E3436);
        p.text_disabled = Color::from_argb(0xFF888A85);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = adwaita_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFE0E0E0);
        p.accent = adwaita_color;
        p.link = adwaita_color;
        p.shadow = Color::from_argb(0x22000000);
        p.dark_shadow = Color::from_argb(0x44000000);
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.background_pressed = Color::from_argb(0xFFECECEC);
        p.background_hovered = Color::from_argb(0xFFF5F5F5);
        p.success = Color::from_argb(0xFF2E7D32);
        p.warning = Color::from_argb(0xFFFBC02D);
        p.error = Color::from_argb(0xFFC62828);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF2E3436);
        p.base = Color::from_argb(0xFF3B3B3B);
        p.alternate = Color::from_argb(0xFF4A4A4A);
        p.text = Color::from_argb(0xFFECECEC);
        p.text_disabled = Color::from_argb(0xFF888A85);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = adwaita_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF555555);
        p.accent = adwaita_color;
        p.link = adwaita_color;
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.background_pressed = Color::from_argb(0xFF484848);
        p.background_hovered = Color::from_argb(0xFF565656);
        p.success = Color::from_argb(0xFF2E7D32);
        p.warning = Color::from_argb(0xFFFBC02D);
        p.error = Color::from_argb(0xFFC62828);
        break;
    }
}

Palette Theme::default_palette(ThemeStyle style, ColorScheme scheme) {
    Palette p;
    p.fonts.system = "sans-serif";
    p.fonts.monospace = "monospace";
    p.fonts.size = 14.0f;
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
        if (sf.size > 0) {
            p.fonts.size = std::floor(sf.size * (96.0f / 72.0f));
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
    auto corner_radius = palette.corner_radius + focus_ring_corner_radius;

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
