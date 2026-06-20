// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_base.hpp"
#include "toolkit/button_state.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/types.hpp"
#include "toolkit/utf8.hpp"
#include "toolkit/window.hpp"
#include "toolkit/window_title_bar.hpp"

#include <algorithm>
#include <cmath>

namespace toolkit {

BaseTheme::BaseTheme(ColorScheme scheme, std::optional<Palette> p) {
    if (p) {
        this->palette = std::move(*p);
    } else {
        this->palette = default_palette(scheme);
    }

    if (palette.fonts.size <= 0) {
        palette.fonts.size = 14.0f;
    }
    if (palette.fonts.system.empty()) {
        palette.fonts.system = "sans-serif";
    }
    if (palette.fonts.monospace.empty()) {
        palette.fonts.monospace = "monospace";
    }

    name = "Base";
    style.layout.margins = {8, 8, 8, 8};
    style.layout.spacing = 8.0f;
    style.scrollbar.thickness = 16.0f;
    style.scrollbar.button_size = 16.0f;
    style.scrollbar.padding = {2, 2, 2, 2};
    style.window_decoration = {32, 0, 0, 0};
    style.corner_radius = 4.0f;
}

std::unique_ptr<Widget> BaseTheme::create_title_bar(Window *window) const {
    auto b = std::make_unique<WindowTitleBar>(window);
    b->initializeTitleBar();
    return b;
}

Palette BaseTheme::default_palette(ColorScheme scheme) const {
    Palette p;
    Theme::init_fonts(p);

    // Default values for all colors to avoid black defaults
    p.highlight = Color::from_rgb(0x6750A4);
    p.accent = Color::from_rgb(0x6750A4);
    p.shadow = Color::from_rgb(0x000000);
    p.dark_shadow = Color::from_rgb(0x000000);
    p.success = Color::from_rgb(0x008000);
    p.warning = Color::from_rgb(0x808000);
    p.error = Color::from_rgb(0x800000);
    p.link = Color::from_rgb(0x0000FF);
    p.tooltip = Color::from_rgb(0xFFFFE0);

    p.light = Color::from_rgb(0xFFFFFF);
    p.shadow = Color::from_rgb(0x808080);
    p.dark_shadow = Color::from_rgb(0x000000);

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xFFFBFE);
        p.base = Color::from_rgb(0xFFFFFF);
        p.alternate = Color::from_rgb(0xF2F2F2);
        p.text = Color::from_rgb(0x1C1B1F);
        p.text_disabled = Color::from_rgb(0x9E9E9E);
        p.placeholder = Color::from_rgb(0x79747E);
        p.highlight = p.accent;
        p.highlighted_text = Color::from_rgb(0xFFFFFF);
        p.border = Color::from_rgb(0xE7E0EC);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_rgb(0x1C1B1F);
        p.base = Color::from_rgb(0x1C1B1F);
        p.alternate = Color::from_rgb(0x2A2A2E);
        p.text = Color::from_rgb(0xE6E1E5);
        p.text_disabled = Color::from_rgb(0x606060);
        p.placeholder = Color::from_rgb(0x868686);
        p.highlight = p.accent;
        p.highlighted_text = Color::from_rgb(0xFFFFFF);
        p.border = Color::from_rgb(0x3A3A3E);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        break;
    }
    return p;
}

void BaseTheme::draw_button(Painter &painter, Rect const &rect, std::string_view text,
                            Icon const &icon, WidgetState const &state, bool flat,
                            std::optional<Color> background) const {
    auto &style = this->style;
    auto hovered = state.interaction == ButtonState::Hovered ||
                   state.interaction == ButtonState::ClickedInside;
    auto pressed = state.interaction == ButtonState::ClickedInside || state.checked;
    auto focused = state.focused;
    auto enabled = state.enabled;
    auto border_c =
        (focused || hovered || (pressed && !state.checked)) ? palette.accent : palette.border;
    if (state.checked) {
        border_c = palette.accent;
    }
    auto text_c = enabled ? palette.text : palette.text_disabled;
    auto text_offset = (style.beveled && pressed && enabled) ? 1.0f : 0.0f;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto text_w = painter.measure_text(text, palette.fonts.size).width;
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
    if (flat && state.interaction != ButtonState::Hovered) {
        defaultBg =
            state.window_active ? palette.window : palette.window_inactive.value_or(palette.window);
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
        auto use_shadow = style.button.bottom_shadow && !hovered && !pressed;
        painter.draw_filled_frame(rect, bg, border_c, palette, pressed && enabled, use_shadow);
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
    painter.draw_text(text, text_pos, text_c, palette.fonts.size);
}

void BaseTheme::draw_checkbox(Painter &painter, Rect const &rect, std::string_view text,
                              CheckState check_state, WidgetState const &state) const {
    auto &style = this->style;
    auto focused = state.focused;
    auto enabled = state.enabled;
    auto hovered = state.interaction == ButtonState::Hovered ||
                   state.interaction == ButtonState::ClickedInside;
    auto pressed = state.interaction == ButtonState::ClickedInside;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto box = style.toggle.box_size;
    auto box_y = (rect.height - box) / 2.0f;
    auto box_rect = Rect{rect.x, box_y, box, box};
    auto border = focused ? palette.accent : palette.border;
    auto bg = palette.base;
    if (enabled) {
        if (pressed) {
            bg = palette.alternate;
        } else if (state.interaction == ButtonState::ClickedOutside) {
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
        painter.draw_line({cx + box * 0.18f, cy + box * 0.2f}, {cx + box * 0.55f, cy - box * 0.25f},
                          palette.text, lw);
    } else if (check_state == CheckState::Partial) {
        auto gap = box * 0.25f;
        auto inner = box_rect.inset(gap);
        painter.fill_rect(inner, palette.text_disabled);
    }

    auto text_x = rect.x + box + style.toggle.spacing;
    auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_c = enabled ? palette.text : palette.text_disabled;
    painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);
}
void BaseTheme::draw_radio_button(Painter &painter, Rect const &rect, std::string_view text,
                                  bool checked, WidgetState const &state) const {
    auto &style = this->style;
    auto focused = state.focused;
    auto enabled = state.enabled;
    auto hovered = state.interaction == ButtonState::Hovered ||
                   state.interaction == ButtonState::ClickedInside;
    auto pressed = state.interaction == ButtonState::ClickedInside;
    auto fm = painter.font_metrics(palette.fonts.size);
    // Use the full row height for the circle so it is 100 % of the widget height.
    // Center X accounts for the radius; center Y must include rect.y so the circle
    // stays correctly placed when the widget is not at the top of its parent.
    auto r = rect.height / 2.0f;
    auto center = Point{rect.x + r, rect.y + rect.height / 2.0f};
    auto text_x = rect.x + rect.height + style.toggle.spacing;
    auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto border = focused ? palette.accent : palette.border;
    auto bg = palette.base;
    if (enabled) {
        if (pressed) {
            bg = palette.alternate;
        } else if (state.interaction == ButtonState::ClickedOutside) {
            bg = palette.base;
        } else if (hovered) {
            bg = palette.alternate;
        }
    }

    // FillEllipse fills up to the boundary; DrawEllipse centers the stroke on it
    // (half inside, half outside).  Shrink the fill by half the border width so
    // the fill sits cleanly inside the outline with no gap or bleed.
    auto hw = style.border_width * 0.5f;
    if (style.toggle.accent_fill) {
        // Windows 11 style: checked = accent fill + accent ring + base-color dot.
        // Unchecked = normal background fill with the standard border.
        auto fill_color = checked ? palette.accent : bg;
        auto ring_color = checked ? palette.accent : border;
        painter.fill_circle(center, r - hw, fill_color);
        painter.draw_circle(center, r - hw, ring_color, style.border_width);
        if (style.beveled && !checked) {
            painter.draw_circle(center, r - hw - 1.0f, palette.shadow, 1.0f);
        }
        if (checked) {
            painter.fill_circle(center, (r - hw) * 0.45f, palette.base);
        }
    } else {
        // Classic style: normal background fill, border ring, accent dot when checked.
        painter.fill_circle(center, r - hw, bg);
        painter.draw_circle(center, r - hw, border, style.border_width);
        if (style.beveled) {
            painter.draw_circle(center, r - hw - 1.0f, palette.shadow, 1.0f);
        }
        if (checked) {
            painter.fill_circle(center, (r - hw) * 0.45f, palette.accent);
        }
    }

    auto text_c = enabled ? palette.text : palette.text_disabled;
    painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size);
}

void BaseTheme::draw_line_input(Painter &painter, Rect const &rect, std::string_view text,
                                std::string_view placeholder, int cursor_pos, int selection_start,
                                int selection_end, WidgetState const &state, bool password_mode,
                                float scroll_offset, std::optional<Color> background,
                                bool cursor_visible, float right_inset) const {
    auto &style = this->style.lineInput;
    auto focused = state.focused;
    auto enabled = state.enabled;
    auto border = focused ? palette.accent : palette.border;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto content_x = style.padding.left;
    auto ri = (right_inset >= 0) ? right_inset : style.padding.right;
    auto content_w = rect.width - content_x - ri;
    auto tx = rect.x + content_x - scroll_offset;

    // RTL: right-align text within the content area
    auto is_rtl = false;
    if (!text.empty() && !password_mode) {
        auto rtl_positions = painter.text_cursor_positions(text, palette.fonts.size);
        // positions[0] > positions[1] means the first character's leading edge is to
        // the right of the next character's → RTL.
        if (rtl_positions.size() > 1) {
            is_rtl = rtl_positions[0] > rtl_positions[1];
        }
    } else if (text.empty() && painter.text_direction() == Painter::TextDirection::RTL) {
        is_rtl = true;
    }
    if (is_rtl) {
        // RTL paragraph: GDI+ renders with the RIGHT edge at the drawing position,
        // extending leftward. Position the right edge at the content area's right edge.
        tx = static_cast<float>(rect.x + content_x + content_w);
    }

    painter.draw_filled_frame(rect, palette.base, border, palette, true);

    auto clip_rect = Rect{rect.x + content_x, rect.y, content_w, rect.height};
    painter.push_clip(clip_rect);

    auto text_c = enabled ? palette.text : palette.text_disabled;
    if (selection_start >= 0 && selection_end > selection_start) {
        auto positions = painter.text_cursor_positions(text, palette.fonts.size);
        auto sx = tx + positions[selection_start];
        auto ex = tx + positions[selection_end];
        if (sx > ex) {
            std::swap(sx, ex);
        }
        auto hy = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
        auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
        painter.fill_rect(
            {static_cast<float>(sx), hy, static_cast<float>(ex - sx), fm.height + 2.0f}, sel_bg);
    }

    if (text.empty() && !focused) {
        painter.draw_text(placeholder, {tx, baseline_y}, palette.placeholder, palette.fonts.size);
    } else if (!text.empty()) {
        if (password_mode) {
            auto dot_radius = palette.fonts.size * 0.25f;
            auto char_w = painter.measure_text("8", palette.fonts.size).width;
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
            if (is_rtl) {
                painter.set_text_direction_rtl(true);
            }
            painter.draw_text(text, {tx, baseline_y}, text_c, palette.fonts.size);
        }
    }

    if (focused && cursor_pos >= 0 && cursor_visible) {
        auto positions = painter.text_cursor_positions(text, palette.fonts.size);
        auto cx = tx + static_cast<float>(positions[cursor_pos]);
        auto cy_top = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
        auto cy_bot = cy_top + fm.height + 2.0f;
        painter.draw_line({cx, cy_top}, {cx, cy_bot}, palette.text, 1.5f);
    }
    painter.pop_clip();
}

void BaseTheme::draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title,
                                  bool hovered, bool active, bool show_mnemonics,
                                  int mnemonic_index) const {
    auto padding = style.menuBar.padding;
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
            before.empty() ? 0.0f : painter.measure_text(before, palette.fonts.size).width;
        auto ch_w = painter.measure_text(ch, palette.fonts.size).width;
        auto ul_y = baseline + fm.descent * 0.4f;

        painter.draw_line({rect.x + padding.left + before_w, ul_y},
                          {rect.x + padding.left + before_w + ch_w, ul_y}, text_c, 1.0f);
    }
}

void BaseTheme::draw_menubar_background(Painter &painter, Rect const &rect,
                                        WidgetState const &state) const {
    // FIXME: do we want a different background for menubar?
    auto bg =
        state.window_active ? palette.window : palette.window_inactive.value_or(palette.window);
    painter.fill_rect(rect, bg);
    if (style.chrome_lines) {
        auto border_c = palette.border;

        painter.draw_line({rect.x, 0}, {rect.x + rect.width, 1.0f}, border_c, style.border_width);
        painter.draw_line({rect.x, rect.height - 1.0f}, {rect.x + rect.width, rect.height - 1.0f},
                          border_c, style.border_width);
    }
}

void BaseTheme::draw_menu_background(Painter &painter, Rect const &rect) const {
    auto shadow = palette.shadow;
    painter.fill_rounded_rect({rect.x + 1, rect.y + 1, rect.width, rect.height}, shadow,
                              style.corner_radius);
    painter.fill_rounded_rect(rect, palette.base, style.corner_radius);
    painter.draw_rounded_rect(rect, palette.border, style.corner_radius, style.border_width);
}

void BaseTheme::draw_menu_item(Painter &painter, Rect const &rect, std::string_view text,
                               Icon const &icon, std::string_view shortcut, bool hovered,
                               bool enabled, bool checkable, bool checked) const {
    auto fm = painter.font_metrics(palette.fonts.size);
    auto baseline = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_col = palette.text;

    if (hovered && enabled) {
        // FIXME: highlight hovered is it needed?
        painter.fill_rounded_rect(rect, palette.highlight, style.corner_radius * 0.5f);
    }

    if (checkable && checked) {
        auto check_rect = Rect{rect.x + 4, rect.y + (rect.height - 12) / 2, 12, 12};
        painter.fill_rounded_rect(check_rect, palette.border, 2.0f);
    }

    auto icon_x = rect.x + style.menu.padding.left + 4;
    if (icon) {
        auto icon_y = rect.y + (rect.height - static_cast<float>(icon->height)) / 2.0f;
        painter.draw_image(*icon, Point{icon_x, icon_y});
        icon_x += static_cast<float>(icon->width) + 4;
    }

    auto text_x = icon_x;
    if (!enabled) {
        // FIXME: remove this alpha channel changing
        text_col.a *= 0.4f;
    }
    painter.draw_text(text, {text_x, baseline}, text_col, palette.fonts.size);

    if (!shortcut.empty()) {
        auto shortcut_w = painter.measure_text(shortcut, palette.fonts.size).width;
        auto shortcut_x = rect.x + rect.width - style.menu.padding.right - shortcut_w - 10.0f;
        painter.draw_text(shortcut, {shortcut_x, baseline}, text_col, palette.fonts.size);
    }
}

void BaseTheme::draw_menu_separator(Painter &painter, Rect const &rect) const {
    auto mid_y = rect.y + rect.height / 2.0f;
    auto sep_col = palette.border;
    painter.draw_line({rect.x + 8, mid_y}, {rect.x + rect.width - 8, mid_y}, sep_col, 0.5f);
}

void BaseTheme::draw_menu_indicator(Painter &painter, Rect const &rect, bool enabled) const {
    auto ind_w = style.button.menu_indicator_width;
    auto cx = rect.x + rect.width - 8.0f;
    auto cy = rect.y + rect.height / 2.0f;
    auto s = ind_w / 2.0f;
    auto color = enabled ? palette.text : palette.text_disabled;
    painter.fill_triangle({cx - s, cy - s + 2.0f}, {cx + s, cy - s + 2.0f}, {cx, cy + s - 2.0f},
                          color);
}

void BaseTheme::draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                                  WidgetState const &state) const {
    auto enabled = state.enabled;
    auto bg = palette.window;
    auto fill = palette.accent;

    auto inner = rect.inset(style.border_width);
    auto fill_w = inner.width * std::clamp(progress, 0.0f, 1.0f);
    auto fill_rect = Rect{inner.x, inner.y, fill_w, inner.height};
    painter.fill_rect(fill_rect, fill);
}

void BaseTheme::draw_slider(Painter &painter, Rect const &rect, float value, bool horizontal,
                            WidgetState const &state) const {
    auto &slider_style = this->style.slider;
    auto hovered = state.interaction == ButtonState::Hovered ||
                   state.interaction == ButtonState::ClickedInside;
    auto pressed = state.interaction == ButtonState::ClickedInside;

    auto groove_rect = Rect{};
    auto handle_rect = Rect{};
    auto v = std::clamp(value, 0.0f, 1.0f);

    if (horizontal) {
        auto groove_y = rect.y + (rect.height - slider_style.groove_thickness) / 2.0f;
        groove_rect = {rect.x, groove_y, rect.width, slider_style.groove_thickness};
        auto track_len = rect.width - slider_style.handle_size;
        auto handle_x = rect.x + slider_style.handle_size / 2.0f + track_len * v;
        handle_rect = {handle_x - slider_style.handle_size / 2.0f,
                       rect.y + (rect.height - slider_style.handle_size) / 2.0f,
                       slider_style.handle_size, slider_style.handle_size};
    } else {
        auto groove_x = rect.x + (rect.width - slider_style.groove_thickness) / 2.0f;
        groove_rect = {groove_x, rect.y, slider_style.groove_thickness, rect.height};
        auto track_len = rect.height - slider_style.handle_size;
        // Vertical slider: 0 is at bottom (local height - offset)
        // Slider::pos_to_value uses: offset = length - p - h_size / 2;
        // So p = length - h_size / 2 - offset
        // where offset = ratio * track_len
        auto handle_y = rect.y + rect.height - slider_style.handle_size / 2.0f - track_len * v;
        handle_rect = {rect.x + (rect.width - slider_style.handle_size) / 2.0f,
                       handle_y - slider_style.handle_size / 2.0f, slider_style.handle_size,
                       slider_style.handle_size};
    }

    painter.fill_rounded_rect(groove_rect, palette.border, slider_style.groove_thickness / 2.0f);

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
    painter.fill_rounded_rect(handle_rect, bg, slider_style.handle_size / 4.0f);
    painter.draw_rounded_rect(handle_rect, palette.border, slider_style.handle_size / 4.0f, 1.0f);
}

void BaseTheme::draw_tab_bar_background(Painter &painter, Rect const &rect,
                                        WidgetState const &state) const {
    painter.fill_rect(rect, palette.tab_background);
}

void BaseTheme::draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                         WidgetState const &state, TabOrientation orientation, bool has_close,
                         bool hovered_close) const {
    auto &style = this->style;
    auto hovered = state.interaction == ButtonState::Hovered;

    auto bg = active ? palette.tab_select_background : palette.tab_background;
    if (hovered) {
        if (palette.background_hovered) {
            bg = active ? Color::lerp(bg, *palette.background_hovered, 0.5f)
                        : *palette.background_hovered;
        } else {
            bg = bg.luma() > 0.5f ? bg.darken(0.05f) : bg.lighten(0.05f);
        }
    }

    auto text_c = state.enabled ? palette.text : palette.text_disabled;
    auto effective_rect =
        style.tabWidget.tab_padding > 0 ? rect.inset(style.tabWidget.tab_padding) : rect;

    bool use_rounded = active || (hovered && style.tabWidget.tab_fully_rounded);
    if (use_rounded) {
        painter.fill_rounded_rect(effective_rect, bg, style.tabWidget.tab_radius);

        // Re-add connecting line logic, but only if there is no padding
        if (active && !style.tabWidget.tab_fully_rounded && style.tabWidget.tab_padding == 0.0f) {
            float r = style.tabWidget.tab_radius;
            float bw = style.border_width;
            if (r > 0 || bw > 0) {
                float fill_r = std::max(r, bw);
                switch (orientation) {
                case TabOrientation::North:
                    painter.fill_rect(
                        {rect.x, rect.y + rect.height - fill_r, rect.width, fill_r + bw}, bg);
                    break;
                case TabOrientation::South:
                    painter.fill_rect({rect.x, rect.y - bw, rect.width, fill_r + bw}, bg);
                    break;
                case TabOrientation::West:
                case TabOrientation::WestVertical:
                    painter.fill_rect(
                        {rect.x + rect.width - fill_r, rect.y, fill_r + bw, rect.height}, bg);
                    break;
                case TabOrientation::East:
                case TabOrientation::EastVertical:
                    painter.fill_rect({rect.x - bw, rect.y, fill_r + bw, rect.height}, bg);
                    break;
                }
            }
        }
    } else {
        painter.fill_rect(effective_rect, bg);
    }

    auto vertical = (orientation == TabOrientation::West || orientation == TabOrientation::East ||
                     orientation == TabOrientation::WestVertical ||
                     orientation == TabOrientation::EastVertical);
    auto text_orientation = Painter::TextOrientation::Horizontal;
    if (orientation == TabOrientation::West) {
        text_orientation = Painter::TextOrientation::VerticalCCW;
    } else if (orientation == TabOrientation::East) {
        text_orientation = Painter::TextOrientation::VerticalCW;
    }

    auto fm = painter.font_metrics(palette.fonts.size);
    auto text_w = painter.measure_text(text, palette.fonts.size).width;
    auto text_x = 0.0f, baseline_y = 0.0f;
    auto close_cx = 0.0f, close_cy = 0.0f;
    auto const close_btn_size = 14.0f;
    auto const close_btn_gap = 6.0f;

    if (vertical) {
        if (orientation == TabOrientation::West || orientation == TabOrientation::East) {
            if (orientation == TabOrientation::West) {
                text_x = effective_rect.x + (effective_rect.width - fm.height) / 2.0f + fm.ascent;
                baseline_y =
                    effective_rect.y + effective_rect.height - style.tabWidget.tab_padding_h;
            } else {
                text_x = effective_rect.x + (effective_rect.width + fm.height) / 2.0f - fm.ascent;
                baseline_y = effective_rect.y + style.tabWidget.tab_padding_h;
            }
            close_cx = effective_rect.x + effective_rect.width / 2.0f;
            close_cy =
                (orientation == TabOrientation::West)
                    ? (effective_rect.y + style.tabWidget.tab_padding_v + close_btn_size / 2.0f)
                    : (effective_rect.y + effective_rect.height - style.tabWidget.tab_padding_v -
                       close_btn_size / 2.0f);
        } else {
            // Standard WestVertical/EastVertical (Horizontal Text)
            if (orientation == TabOrientation::WestVertical) {
                // Bar on the left, content on the right. Button on the right.
                text_x = effective_rect.x + style.tabWidget.tab_padding_h;
                baseline_y =
                    effective_rect.y + (effective_rect.height - fm.height) / 2.0f + fm.ascent;
                close_cx = effective_rect.x + effective_rect.width - style.tabWidget.tab_padding_h -
                           close_btn_size / 2.0f;
                close_cy = effective_rect.y + effective_rect.height / 2.0f;
            } else {
                // Bar on the right, content on the left. Button on the left.
                if (has_close) {
                    close_cx =
                        effective_rect.x + style.tabWidget.tab_padding_h + close_btn_size / 2.0f;
                    text_x = effective_rect.x + style.tabWidget.tab_padding_h + close_btn_size +
                             close_btn_gap;
                } else {
                    text_x = effective_rect.x + style.tabWidget.tab_padding_h;
                }
                baseline_y =
                    effective_rect.y + (effective_rect.height - fm.height) / 2.0f + fm.ascent;
                close_cy = effective_rect.y + effective_rect.height / 2.0f;
            }
        }
    } else {
        text_x = effective_rect.x + style.tabWidget.tab_padding_h;
        baseline_y = effective_rect.y + (effective_rect.height - fm.height) / 2.0f + fm.ascent;
        close_cx = effective_rect.x + style.tabWidget.tab_padding_h + text_w + close_btn_gap +
                   close_btn_size / 2.0f;
        close_cy = effective_rect.y + effective_rect.height / 2.0f;
    }

    painter.draw_text(text, {text_x, baseline_y}, text_c, palette.fonts.size, FontFamily::System,
                      text_orientation);

    if (has_close) {
        auto close_rect = Rect{close_cx - close_btn_size / 2.0f, close_cy - close_btn_size / 2.0f,
                               close_btn_size, close_btn_size};

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

    if (active && style.tabWidget.indicator_weight.has_value() &&
        *style.tabWidget.indicator_weight != 0.0f) {
        auto const weight = std::abs(*style.tabWidget.indicator_weight);
        auto const on_inner = *style.tabWidget.indicator_weight > 0.0f;
        auto indicator = Rect{};
        auto r2 = rect;

        if (vertical) {
            if (!on_inner) {
                r2.y += style.tabWidget.tab_radius;
                r2.height -= style.tabWidget.tab_radius * 2;
            }
        } else {
            if (!on_inner) {
                r2.x += style.tabWidget.tab_radius;
                r2.width -= style.tabWidget.tab_radius * 2;
            }
        }

        if (orientation == TabOrientation::North) {
            indicator = {r2.x, on_inner ? rect.y + rect.height - weight : rect.y, r2.width, weight};
        } else if (orientation == TabOrientation::South) {
            indicator = {r2.x, on_inner ? rect.y : rect.y + rect.height - weight, r2.width, weight};
        } else if (orientation == TabOrientation::West ||
                   orientation == TabOrientation::WestVertical) {
            indicator = {on_inner ? rect.x + rect.width - weight : rect.x, r2.y, weight, r2.height};
        } else if (orientation == TabOrientation::East ||
                   orientation == TabOrientation::EastVertical) {
            indicator = {on_inner ? rect.x : rect.x + rect.width - weight, r2.y, weight, r2.height};
        }
        painter.fill_rect(indicator, palette.accent);
    }
}

void BaseTheme::draw_list_item(Painter &painter, Rect const &rect, std::string_view text,
                               Icon const &icon, bool selected, bool hovered,
                               bool alternate) const {
    auto const &style = this->style.listView;
    auto bg = palette.base;

    auto is_dark = palette.window.luma() < 0.5f;
    auto alt_color = is_dark ? palette.base.lighten(0.03f) : palette.base.darken(0.02f);

    if (selected) {
        bg = palette.highlight;
    } else if (hovered) {
        // FIXME: don't compute colors. Use colors from palette.
        bg = Color::lerp(alt_color, palette.highlight, 0.5);
    } else if (alternate) {
        bg = alt_color;
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

void BaseTheme::draw_icon_grid_item(Painter &painter, Rect const &rect, std::string_view text,
                                    Icon const &icon, bool selected, bool hovered, int icon_size,
                                    bool scale) const {
    if (selected) {
        painter.fill_rounded_rect(rect, palette.highlight, 4.0f);
    } else if (hovered) {
        auto h = palette.highlight;
        h.a = 0.2f;
        painter.fill_rounded_rect(rect, h, 4.0f);
    }

    auto icon_area_x = rect.x + (rect.width - static_cast<float>(icon_size)) / 2.0f;
    auto icon_area_y = rect.y + 4.0f;
    if (icon) {
        // Scale if explicitly requested, or whenever the actual icon dimensions
        // don't match the requested cell size — this handles both the scale-down
        // case (e.g. only a 48 px variant is available but 32 px was asked for,
        // which would otherwise bleed outside the cell) and the scale-up case
        // (e.g. only a 16 px variant is available but 48 px was asked for,
        // which would otherwise render as a tiny icon in a large empty cell).
        auto size_mismatch = icon->width != icon_size || icon->height != icon_size;
        if (scale || size_mismatch) {
            painter.draw_image_scaled(*icon,
                                      {icon_area_x, icon_area_y, static_cast<float>(icon_size),
                                       static_cast<float>(icon_size)});
        } else {
            // Exact match: draw at natural size (centering offset is zero).
            painter.draw_image(*icon, {icon_area_x, icon_area_y});
        }
    }

    auto fm = painter.font_metrics(palette.fonts.size);
    auto display_text = std::string(text);
    auto text_w = painter.measure_text(display_text, palette.fonts.size).width;
    auto max_text_w = rect.width - 4.0f;
    if (text_w > max_text_w) {
        auto suffix = std::string_view{"..."};
        auto sw = painter.measure_text(suffix, palette.fonts.size).width;
        if (sw < max_text_w) {
            while (!display_text.empty() && text_w + sw > max_text_w) {
                size_t last_pos = Utf8Iterator::prev(display_text, display_text.size());
                display_text.erase(last_pos);
                text_w = painter.measure_text(display_text, palette.fonts.size).width;
            }
            display_text += suffix;
            text_w = painter.measure_text(display_text, palette.fonts.size).width;
        }
    }

    auto text_x = rect.x + (rect.width - text_w) / 2.0f;
    auto text_y = icon_area_y + static_cast<float>(icon_size) + 4.0f + fm.ascent;
    auto text_c = selected ? palette.highlighted_text : palette.text;
    painter.draw_text(display_text, Point{text_x, text_y}, text_c, palette.fonts.size);
}

void BaseTheme::draw_list_background(Painter &painter, Rect const &rect,
                                     WidgetState const &state) const {
    painter.draw_filled_frame(rect, palette.base, palette.border, palette, true);
}

void BaseTheme::draw_table_background(Painter &painter, Rect const &rect,
                                      WidgetState const &state) const {
    painter.draw_filled_frame(rect, palette.base, palette.border, palette, true);
}

void BaseTheme::draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                               bool has_children, bool expanded, bool selected, bool hovered,
                               bool alternate) const {
    auto const &tree_style = this->style.treeView;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto x_offset = tree_style.item_padding_h + depth * tree_style.indent;

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
    x_offset += tree_style.indent + 4.0f;

    auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_col = selected ? palette.highlighted_text : palette.text;
    painter.draw_text(text, {x_offset, text_y}, text_col, palette.fonts.size);
}

void BaseTheme::draw_tree_background(Painter &painter, Rect const &rect,
                                     WidgetState const &state) const {
    if (style.beveled) {
        painter.draw_filled_frame(rect, palette.base, palette.border, palette, true);
    } else {
        painter.fill_rounded_rect(rect, palette.base, style.corner_radius);
        if (style.border_width > 0) {
            auto bw = style.border_width;
            auto inset = bw / 2.0f;
            painter.draw_rounded_rect(rect.inset(inset), palette.border,
                                      std::max(0.0f, style.corner_radius - inset), bw);
        }
    }
}

void BaseTheme::draw_combobox(Painter &painter, Rect const &rect, std::string_view text,
                              WidgetState const &state, bool open) const {
    auto const &style = this->style.combo;
    auto border = state.focused ? palette.accent : palette.border;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto baseline_y = (rect.height - fm.height) / 2.0f + fm.ascent;

    painter.draw_filled_frame(rect, palette.base, border, palette, true);
    if (!text.empty()) {
        auto clip_w = rect.width - style.padding.left - style.padding.right - 16.0f;
        painter.push_clip({style.padding.left, 0, clip_w, rect.height});
        painter.draw_text(text, {style.padding.left, baseline_y}, palette.text, palette.fonts.size);
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

void BaseTheme::draw_combobox_item(Painter &painter, Rect const &rect, std::string_view text,
                                   bool hovered) const {
    auto const &style = this->style.combo;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto baseline = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto tc = hovered ? palette.highlighted_text : palette.text;

    if (hovered) {
        painter.fill_rect(rect, palette.highlight);
    }

    painter.draw_text(text, {rect.x + style.padding.left, baseline}, tc, palette.fonts.size);
}

void BaseTheme::draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const {
    auto fm = painter.font_metrics(palette.fonts.size);
    // auto const &style = this->style.tooltip;
    auto text_x = rect.x + style.tooltip.padding;
    auto baseline_y = rect.y + style.tooltip.padding + fm.ascent;

    painter.fill_rounded_rect(rect, palette.tooltip, style.corner_radius);
    painter.draw_rounded_rect(rect, palette.border, style.corner_radius, style.border_width);
    painter.draw_text(text, {text_x, baseline_y}, palette.text, palette.fonts.size);
}

void BaseTheme::draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                                   WidgetState const &state) const {
    auto bg = palette.window;
    if (state.interaction == ButtonState::ClickedInside) {
        bg = palette.border;
    } else if (state.interaction == ButtonState::Hovered) {
        bg = palette.alternate;
    }

    painter.fill_rect(rect, bg);

    auto center = Point{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
    auto size = 5.0f;

    if (button == DecorationButton::Minimize) {
        painter.draw_line({center.x - size, center.y}, {center.x + size, center.y}, palette.text,
                          1.5f);
    } else if (button == DecorationButton::Maximize) {
        painter.draw_rect({center.x - size, center.y - size, size * 2, size * 2}, palette.text,
                          1.5f);
    } else if (button == DecorationButton::Restore) {
        painter.draw_rect({center.x - size, center.y - size, size * 1.5f, size * 1.5f},
                          palette.text, 1.5f);
        painter.draw_rect(
            {center.x - size + 0.5f, center.y - size + 0.5f, size * 1.5f, size * 1.5f},
            palette.text, 1.5f);
    } else if (button == DecorationButton::Close) {
        painter.draw_line({center.x - size, center.y - size}, {center.x + size, center.y + size},
                          palette.text, 1.5f);
        painter.draw_line({center.x + size, center.y - size}, {center.x - size, center.y + size},
                          palette.text, 1.5f);
    }
}

void BaseTheme::draw_tab_content_background(Painter &painter, Rect const &rect) const {
    painter.fill_rect(rect, palette.window);
}

void BaseTheme::draw_toolbar(Painter &painter, Rect const &rect, WidgetState const &state) const {
    auto bg =
        state.window_active ? palette.window : palette.window_inactive.value_or(palette.window);
    auto bw = style.border_width;
    if (style.beveled) {
        painter.draw_line({rect.x, rect.y + rect.height - 1.0f},
                          {rect.x + rect.width, rect.y + rect.height - 1.0f}, palette.shadow, bw);
    } else if (style.chrome_lines) {
        auto border_c = bg.darken(0.15f);
        painter.draw_line({rect.x, rect.y + rect.height - 1.0f},
                          {rect.x + rect.width, rect.y + rect.height - 1.0f}, border_c, bw);
    }
}

void BaseTheme::draw_spinbox(Painter &painter, Rect const &rect, std::string_view text,
                             int cursor_pos, int selection_start, int selection_end,
                             WidgetState const &state, bool hovered_up, bool pressed_up,
                             bool hovered_down, bool pressed_down, bool cursor_visible) const {
    auto const &style = this->style.lineInput;
    auto focused = state.focused;
    auto enabled = state.enabled;
    auto bw = rect.height;
    auto field_rect = Rect{rect.x, rect.y, rect.width - bw, rect.height};
    auto border = focused ? palette.accent : palette.border;

    painter.draw_filled_frame(field_rect, palette.base, border, palette, true);

    auto content_x = field_rect.x + style.padding.left;
    auto content_w = field_rect.width - style.padding.left - style.padding.right;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto baseline_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;

    // RTL: right-align text within the content area
    auto is_rtl = false;
    if (!text.empty()) {
        auto rtl_positions = painter.text_cursor_positions(text, palette.fonts.size);
        is_rtl = rtl_positions.size() > 1 && rtl_positions[0] > rtl_positions[text.size()];
        if (is_rtl) {
            content_x = static_cast<float>(field_rect.x + style.padding.left + content_w);
        }
    }

    auto clip_rect = Rect{content_x, rect.y, content_w, rect.height};
    painter.push_clip(clip_rect);

    if (selection_start >= 0 && selection_end > selection_start) {
        auto positions = painter.text_cursor_positions(text, palette.fonts.size);
        auto sx = content_x + positions[selection_start];
        auto ex = content_x + positions[selection_end];
        if (sx > ex) {
            std::swap(sx, ex);
        }
        auto hy = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
        auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
        painter.fill_rect(
            {static_cast<float>(sx), hy, static_cast<float>(ex - sx), fm.height + 2.0f}, sel_bg);
    }

    auto text_c = enabled ? palette.text : palette.text_disabled;
    if (is_rtl) {
        painter.set_text_direction_rtl(true);
    }
    painter.draw_text(text, {content_x, baseline_y}, text_c, palette.fonts.size);

    if (focused && cursor_pos >= 0 && cursor_visible) {
        auto positions = painter.text_cursor_positions(text, palette.fonts.size);
        auto cx = content_x + static_cast<float>(positions[cursor_pos]);
        auto cy_top = rect.y + (rect.height - fm.height) / 2.0f - 1.0f;
        auto cy_bot = cy_top + fm.height + 2.0f;
        painter.draw_line({cx, cy_top}, {cx, cy_bot}, palette.text, 1.5f);
    }

    painter.pop_clip();
}

void BaseTheme::draw_text_edit(Painter &painter, Rect const &rect,
                               std::span<std::string const> lines, int cursor_line, int cursor_col,
                               int selection_start_line, int selection_start_col,
                               int selection_end_line, int selection_end_col,
                               int first_visible_line, float line_height, float gutter_width,
                               float scroll_x, float scroll_y, WidgetState const &state,
                               std::chrono::steady_clock::time_point cursor_blink_time) const {
    auto focused = state.focused;
    auto enabled = state.enabled;
    auto fm = painter.font_metrics(palette.fonts.size, FontFamily::Monospace);
    auto bg = palette.base;
    auto border = focused ? palette.accent : palette.border;
    auto text_c = enabled ? palette.text : palette.text_disabled;

    painter.draw_filled_frame(rect, bg, border, palette, true);

    auto last = std::min(static_cast<int>(lines.size()) - 1,
                         first_visible_line + static_cast<int>(rect.height / line_height));

    auto gutter_rect = Rect{rect.x, rect.y, gutter_width, rect.height};
    auto gutter_bg = palette.base;
    painter.fill_rect(gutter_rect, gutter_bg);

    for (auto i = first_visible_line; i <= last; i++) {
        auto y = rect.y + line_height * static_cast<float>(i - first_visible_line);
        auto baseline = y + (line_height - fm.height) / 2.0f + fm.ascent;
        auto num = std::to_string(i + 1);
        auto nw = painter.measure_text(num, palette.fonts.size, FontFamily::Monospace).width;
        painter.draw_text(num, {rect.x + gutter_width - nw - 8.0f, baseline}, palette.placeholder,
                          palette.fonts.size, FontFamily::Monospace);
    }

    auto area = Rect{rect.x + gutter_width, rect.y, rect.width - gutter_width, rect.height};
    auto tx0 = rect.x + gutter_width - scroll_x;
    // FIXME - hardcoded color
    auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
    auto has_sel =
        selection_start_line >= 0 && selection_end_line >= 0 &&
        (selection_start_line < selection_end_line ||
         (selection_start_line == selection_end_line && selection_start_col < selection_end_col));

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
                auto sx = tx0 + (sel_start > 0
                                     ? painter
                                           .measure_text(lines[i].substr(0, sel_start),
                                                         palette.fonts.size, FontFamily::Monospace)
                                           .width
                                     : 0.0f);
                auto ex = tx0 + painter
                                    .measure_text(lines[i].substr(0, sel_end), palette.fonts.size,
                                                  FontFamily::Monospace)
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
            auto cy = rect.y + line_height * static_cast<float>(cursor_line - first_visible_line);
            auto cx = tx0;
            if (cursor_col > 0 && cursor_line < static_cast<int>(lines.size())) {
                cx += painter
                          .measure_text(lines[cursor_line].substr(0, cursor_col),
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
}

void BaseTheme::draw_scrollbar(Painter &painter, Rect const &rect, float value,
                               Orientation orientation, WidgetState const &state,
                               bool hovered_left_btn, bool pressed_left_btn, bool hovered_right_btn,
                               bool pressed_right_btn, bool hovered_thumb) const {
    auto const &style = this->style.scrollbar;
    auto enabled = state.enabled;
    auto horizontal = orientation == Orientation::Horizontal;

    auto r = rect;
    if (horizontal) {
        if (r.height > style.thickness) {
            r.y += (r.height - style.thickness) / 2.0f;
            r.height = style.thickness;
        }
    } else {
        if (r.width > style.thickness) {
            r.x += (r.width - style.thickness) / 2.0f;
            r.width = style.thickness;
        }
    }

    auto bs =
        style.show_buttons ? std::min(horizontal ? r.height : r.width, style.button_size) : 0.0f;

    auto track_rect = horizontal ? Rect{r.x + bs, r.y, r.width - 2 * bs, r.height}
                                 : Rect{r.x, r.y + bs, r.width, r.height - 2 * bs};

    auto thumb_size = std::max(20.0f, (horizontal ? track_rect.width : track_rect.height) * 0.1f);
    auto max_thumb_move = (horizontal ? track_rect.width : track_rect.height) - thumb_size;

    auto thumb = Rect{};
    if (horizontal) {
        auto thumb_h = std::max(1.0f, r.height - style.padding.top - style.padding.bottom);
        auto thumb_x = track_rect.x + value * max_thumb_move;
        auto thumb_y = r.y + (r.height - thumb_h) / 2.0f;
        thumb = {thumb_x, thumb_y, thumb_size, thumb_h};
    } else {
        auto thumb_w = std::max(1.0f, r.width - style.padding.left - style.padding.right);
        auto thumb_x = r.x + (r.width - thumb_w) / 2.0f;
        auto thumb_y = track_rect.y + value * max_thumb_move;
        thumb = {thumb_x, thumb_y, thumb_w, thumb_size};
    }

    auto border_c = state.focused ? palette.accent : palette.border;
    auto track_c = state.focused ? palette.alternate : Color::mid(palette.window, palette.base);

    // Track background
    if (style.show_frame) {
        painter.draw_filled_frame(track_rect, track_c, border_c, palette, true);
    } else {
        painter.fill_rect(track_rect, track_c);
    }

    // Buttons
    auto lbtn = horizontal ? Rect{r.x, r.y + (r.height - bs) / 2.0f, bs, bs}
                           : Rect{r.x + (r.width - bs) / 2.0f, r.y, bs, bs};
    auto rbtn = horizontal ? Rect{r.x + r.width - bs, r.y + (r.height - bs) / 2.0f, bs, bs}
                           : Rect{r.x + (r.width - bs) / 2.0f, r.y + r.height - bs, bs, bs};

    auto draw_btn = [&](Rect const &r, bool pressed, bool hovered, bool is_next) {
        auto bg = pressed ? palette.border : (hovered ? palette.alternate : palette.window);
        painter.draw_filled_frame(r, bg, border_c, palette, pressed);

        auto arrow_c = enabled ? palette.text : palette.text_disabled;
        auto s = bs * 0.25f;
        auto cx = r.x + r.width / 2.0f;
        auto cy = r.y + r.height / 2.0f;

        if (horizontal) {
            if (is_next) {
                painter.fill_triangle({cx + s, cy}, {cx - s, cy - s}, {cx - s, cy + s}, arrow_c);
            } else {
                painter.fill_triangle({cx - s, cy}, {cx + s, cy - s}, {cx + s, cy + s}, arrow_c);
            }
        } else {
            if (is_next) {
                painter.fill_triangle({cx, cy + s}, {cx - s, cy - s}, {cx + s, cy - s}, arrow_c);
            } else {
                painter.fill_triangle({cx, cy - s}, {cx - s, cy + s}, {cx + s, cy + s}, arrow_c);
            }
        }
    };

    draw_btn(lbtn, pressed_left_btn, hovered_left_btn, false);
    draw_btn(rbtn, pressed_right_btn, hovered_right_btn, true);

    // Thumb
    auto thumb_bg = hovered_thumb ? palette.accent : palette.text;
    thumb_bg.a = hovered_thumb ? 0.8f : 0.5f;
    painter.fill_rounded_rect(thumb, thumb_bg, 3.0f);
}

void BaseTheme::draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const {
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

Size BaseTheme::measure_label(std::string_view text, float font_size) const {
    auto *p = detail::current_platform();
    auto fm = p->font_metrics(font_size);
    auto w = p->measure_text(text, font_size).width;
    return {w, fm.height + 4.0f};
}

Size BaseTheme::measure_button(std::string_view text, Icon const &icon) const {
    auto &style = this->style;
    auto *p = detail::current_platform();
    auto fm = p->font_metrics(palette.fonts.size);
    auto text_w = p->measure_text(text, palette.fonts.size).width;
    auto icon_w = 0.0f;
    if (icon) {
        icon_w = static_cast<float>(icon->width);
    }
    auto total_w = text_w + (icon ? icon_w + 4.0f : 0.0f);
    auto w = total_w + style.button.padding.left + style.button.padding.right;
    auto h = fm.height + style.button.padding.top + style.button.padding.bottom;
    return {w, h};
}

Size BaseTheme::measure_checkbox(std::string_view text) const {
    auto &style = this->style;
    auto *p = detail::current_platform();
    auto fm = p->font_metrics(palette.fonts.size);
    auto text_w = p->measure_text(text, palette.fonts.size).width;
    auto w = style.toggle.box_size + style.toggle.spacing + text_w;
    auto h = std::max(style.toggle.box_size, fm.height);
    return Size{w, h};
}

Size BaseTheme::measure_radio_button(std::string_view text) const {
    auto &style = this->style;
    auto *p = detail::current_platform();
    auto fm = p->font_metrics(palette.fonts.size);
    auto text_w = p->measure_text(text, palette.fonts.size).width;
    // Circle diameter equals the row height (fm.height); text follows after spacing.
    auto h = fm.height;
    auto w = h + style.toggle.spacing + text_w;
    return Size{w, h};
}

Size BaseTheme::measure_menubar_item(std::string_view text) const {
    // FIXME: using platform to measure text
    auto &style = this->style;
    auto p = detail::current_platform();
    auto text_w = p->measure_text(text, palette.fonts.size).width;
    return {text_w + style.menuBar.padding.left + style.menuBar.padding.right, 0};
}

Size BaseTheme::measure_menu_item(std::string_view text, Icon const &icon,
                                  std::string_view shortcut) const {
    auto &style = this->style;
    auto *p = detail::current_platform();
    auto w = style.menu.item_padding * 2;
    if (icon) {
        w += static_cast<float>(icon->width) + style.menu.item_padding;
    }
    w += p->measure_text(text, palette.fonts.size).width;
    if (!shortcut.empty()) {
        w += style.menu.item_padding + p->measure_text(shortcut, palette.fonts.size).width;
    }
    auto h = p->font_metrics(palette.fonts.size).height + style.menu.item_padding * 2;
    return {w, h};
}

float BaseTheme::menu_separator_height() const { return 8.0f; }
Size BaseTheme::measure_tab(std::string_view text) const {
    auto &style = this->style;
    auto *p = detail::current_platform();
    auto text_w = p->measure_text(text, palette.fonts.size).width;
    auto w = text_w + style.tabWidget.tab_padding_h * 2;
    auto h = p->font_metrics(palette.fonts.size).height + style.tabWidget.tab_padding_v * 2;
    return {w, h};
}
float BaseTheme::list_item_height() const { return 24.0f; }

Size BaseTheme::measure_icon_grid_item(std::string_view text, int icon_size) const {
    auto *p = detail::current_platform();
    auto text_w = p->measure_text(text, palette.fonts.size).width;
    auto fm = p->font_metrics(palette.fonts.size);

    auto w = std::max(static_cast<float>(icon_size), text_w) + 8.0f;
    auto h = static_cast<float>(icon_size) + fm.height + 12.0f;
    return {w, h};
}

Size BaseTheme::measure_tooltip(std::string_view text) const {
    auto text_w = detail::current_platform()->measure_text(text, palette.fonts.size).width;

    // FIXME: padding is hardcoded for tabs
    auto padding = 0.5f;
    auto w = text_w + padding * 2;
    auto h = palette.fonts.size + padding * 2;
    return {w, h};
}

Margins BaseTheme::button_padding() const { return this->style.button.padding; }
Margins BaseTheme::line_input_padding() const { return this->style.lineInput.padding; }

} // namespace toolkit
