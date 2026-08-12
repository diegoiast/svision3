// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_win95.hpp"
#include "toolkit/button.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include "toolkit/window_title_bar.hpp"
#include <algorithm>
#include <cmath>
#include <memory>

namespace toolkit {

class Win95TitleBar : public WindowTitleBar {
  public:
    using WindowTitleBar::WindowTitleBar;

    void initializeTitleBar() override {
        layout = create_title_layout();
        layout->set_spacing(2.0f);
        layout->set_margins({2, 2, 2, 2});

        title_label = new Label(std::string{window_->title()});
        title_label->set_alignment(Alignment::Start).set_shrinkable(true).set_elide(true);

        layout->add_widget(std::unique_ptr<Label>(title_label), 1);

        auto const &decoration = Theme::current().style.window_decoration;
        auto btn_size = Size{decoration.top - 4.0f, decoration.top - 4.0f};

        min_btn = new TitlebarButton(DecorationButton::Minimize, "Minimize", btn_size);
        min_btn->on_click = [this] { window_->minimize(); };

        max_btn = new TitlebarButton(DecorationButton::Maximize, "Maximize", btn_size);
        max_btn->on_click = [this] {
            if (window_->is_maximized()) {
                window_->restore();
            } else {
                window_->maximize();
            }
        };

        close_btn = new TitlebarButton(DecorationButton::Close, "Close", btn_size);
        close_btn->on_click = [this] { window_->close(); };

        sync_button_states();

        layout->add_widget(std::unique_ptr<Widget>(min_btn));
        layout->add_widget(std::unique_ptr<Widget>(max_btn));

        auto *spacer = new Label("");
        spacer->set_min_size({2, 0});
        layout->add_widget(std::unique_ptr<Widget>(spacer));

        layout->add_widget(std::unique_ptr<Widget>(close_btn));
    }

    void paint(Painter &painter) override {
        auto const &pal = Theme::current().palette;
        auto active = window_->is_active();
        auto bg = active ? pal.highlight : pal.window;
        auto fg = active ? pal.highlighted_text : pal.text_disabled;

        painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);
        sync_button_states();

        if (title_label) {
            title_label->set_text(std::string(window_->title()));
            title_label->set_color(fg);
        }

        layout->paint(painter);
    }
};

Win95Theme::Win95Theme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Windows 95";
    style.ringFocus.margin = 0.0f;
    style.ringFocus.corner_radius = 0.0f;
    style.ringFocus.line_style = Painter::LineStyle::Dotted;

    style.beveled = true;
    style.border_width = 2.0f;
    style.progressBar.height = 20;
    style.inline_scrollbars = false;
    style.window_decoration = {26, 0, 0, 0};
    style.tabWidget.tab_radius = 0.0f;
    style.corner_radius = 0.0f;
}

std::unique_ptr<Widget> Win95Theme::create_title_bar(Window *window) const {
    auto b = std::make_unique<Win95TitleBar>(window);
    b->initializeTitleBar();
    return b;
}

void Win95Theme::draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                                    WidgetState const &state) const {
    auto pressed = state.interaction == ButtonState::ClickedInside;
    auto bg = palette.window;
    auto center = Point{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
    auto symbol_c = palette.text;
    auto s = 4.0f;

    painter.draw_filled_frame(rect, bg, palette.border, palette, pressed);
    if (pressed) {
        center.x += 1.0f;
        center.y += 1.0f;
    }

    switch (button) {
    case DecorationButton::Close:
        painter.draw_line({center.x - s, center.y - s}, {center.x + s, center.y + s}, symbol_c,
                          2.0f);
        painter.draw_line({center.x + s, center.y - s}, {center.x - s, center.y + s}, symbol_c,
                          2.0f);
        break;
    case DecorationButton::Minimize:
        painter.draw_line({center.x - s, center.y + s}, {center.x + s, center.y + s}, symbol_c,
                          2.0f);
        break;
    case DecorationButton::Maximize:
        painter.draw_rect({center.x - s, center.y - s, s * 2, 2.0f}, symbol_c, 1.0f); // Top bar
        painter.draw_rect({center.x - s, center.y - s, s * 2, s * 2}, symbol_c, 1.0f);
        break;
    case DecorationButton::Restore: {
        // Two overlapping rectangles
        painter.draw_rect({center.x - s + 2, center.y - s, s * 2 - 2, 2.0f}, symbol_c, 1.0f);
        painter.draw_rect({center.x - s + 2, center.y - s, s * 2 - 2, s * 2 - 2}, symbol_c, 1.0f);
        painter.draw_rect({center.x - s, center.y - s + 2, s * 2 - 2, 2.0f}, symbol_c, 1.0f);
        painter.draw_rect({center.x - s, center.y - s + 2, s * 2 - 2, s * 2 - 2}, symbol_c, 1.0f);
        break;
    }
    case DecorationButton::Menu:
        // Usually a small icon, but for now a simple dash
        painter.draw_line({center.x - s, center.y}, {center.x + s, center.y}, symbol_c, 2.0f);
        break;
    }
}

Palette Win95Theme::default_palette(ColorScheme scheme) const {
    Palette p = BaseTheme::default_palette(scheme);
    Color windows95_color = Color::from_argb(0xFF000080);

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFC0C0C0);
        p.base = p.window;
        p.alternate = p.window;
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFFACA899);
        p.placeholder = Color::from_argb(0xFFACA899);
        p.highlight = windows95_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFACA899);
        p.accent = windows95_color;
        p.link = windows95_color;
        p.light = Color::from_argb(0xFFFFFFFF);
        p.shadow = Color::from_argb(0xFFACA899);
        p.dark_shadow = Color::from_argb(0xFF716F64);
        p.dark_shadow = Color::from_argb(0xff9922);

        p.background_pressed = Color::from_argb(0xFFA0A0A0);
        p.background_hovered = std::nullopt;
        p.tooltip = Color::from_argb(0xFFFFFFE1);
        p.success = Color::from_argb(0xFF008000);
        p.warning = Color::from_argb(0xFFFF8000);
        p.error = Color::from_argb(0xFF800000);
        p.tab_select_background = p.window;
        p.tab_background = p.window;
        // p.list_hover = Color::from_argb(0xFFD4E8FF);
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF1C1B1F);
        p.base = Color::from_argb(0xFF2A2A2E);
        p.alternate = Color::from_argb(0xFF3A3A3E);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF808080);
        p.placeholder = Color::from_argb(0xFF808080);
        p.highlight = Color::from_argb(0xFF000080);
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF404040);
        p.accent = Color::from_argb(0xFF000080);
        p.link = Color::from_argb(0xFF000080);
        p.light = Color::from_argb(0xFF505050);
        p.shadow = Color::from_argb(0xFF303030);
        p.dark_shadow = Color::from_argb(0xFF101010);
        p.background_pressed = Color::from_argb(0xFF404040);
        p.background_hovered = std::nullopt;
        p.tooltip = Color::from_argb(0xFFFFFFE1);
        p.success = Color::from_argb(0xFF008000);
        p.warning = Color::from_argb(0xFFFF8000);
        p.error = Color::from_argb(0xFF800000);
        p.tab_select_background = p.window;
        p.tab_background = p.window;
        break;
    }
    return p;
}

void Win95Theme::draw_splitter_handle(Painter &painter, float pos, Rect const &splitter_rect,
                                      Orientation orientation, bool hovered) const {
    // Win95 style: beveled grooves (pairs of shadow+highlight lines)
    auto const &s = style.splitter;
    auto const shadow = palette.window.darken(0.4f);
    auto const highlight = palette.window.lighten(0.6f);
    auto const total_len = (s.dot_count - 1) * s.dot_spacing;

    if (orientation == Orientation::Horizontal) {
        auto const mid = splitter_rect.y + splitter_rect.height / 2.0f;
        auto const start = mid - total_len / 2.0f;
        for (int i = 0; i < s.dot_count; ++i) {
            auto y = start + i * s.dot_spacing;
            // 2px shadow + 2px highlight = one bevel groove
            painter.fill_rect({pos - 1.0f, y, 1.0f, s.dot_size}, shadow);
            painter.fill_rect({pos,         y, 1.0f, s.dot_size}, highlight);
        }
    } else {
        auto const mid = splitter_rect.x + splitter_rect.width / 2.0f;
        auto const start = mid - total_len / 2.0f;
        for (int i = 0; i < s.dot_count; ++i) {
            auto x = start + i * s.dot_spacing;
            painter.fill_rect({x, pos - 1.0f, s.dot_size, 1.0f}, shadow);
            painter.fill_rect({x, pos,         s.dot_size, 1.0f}, highlight);
        }
    }
}

void Win95Theme::draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const {
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

void Win95Theme::draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                int depth, bool has_children, bool expanded, bool selected,
                                bool hovered, bool alternate) const {
    auto const &tree_style = this->style.treeView;
    auto const &cb_style = this->style.toggle;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto x_offset = tree_style.item_padding_h + depth * tree_style.indent;

    if (has_children) {
        auto icon_x = x_offset;
        auto box_size = cb_style.box_size;
        auto box_rect = Rect{icon_x, rect.y + (rect.height - box_size) / 2.0f, box_size, box_size};

        painter.draw_filled_frame(box_rect, palette.base, palette.border, palette, false);

        auto expand_collapse_char = expanded ? "-" : "+";
        auto char_w = painter.measure_text(expand_collapse_char, palette.fonts.size).width;
        auto char_x = icon_x + (box_size - char_w) / 2.0f;
        auto char_y = box_rect.y + (box_size - fm.height) / 2.0f + fm.ascent;

        painter.draw_text(expand_collapse_char, Point{char_x, char_y}, palette.text,
                          palette.fonts.size);
    }

    x_offset += tree_style.indent + 4.0f;

    auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_col = selected ? palette.highlighted_text : palette.text;
    painter.draw_text(text, Point{x_offset, text_y}, text_col, palette.fonts.size);
}

void Win95Theme::draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                                   WidgetState const &state) const {
    auto const chunk_width = 8.0f;
    auto const chunk_gap = 2.0f;
    auto enabled = state.enabled;
    auto bg = enabled ? palette.window : palette.window.darken(0.1f);
    auto fill_c = enabled ? palette.accent : palette.accent.darken(0.2f);
    auto inner = rect.inset(Theme::current().style.border_width);
    auto chunk_count = static_cast<int>(inner.width / (chunk_width + chunk_gap));
    auto fill_w = inner.width * std::clamp(progress, 0.0f, 1.0f);

    for (auto i = 0; i < chunk_count; ++i) {
        auto cx = inner.x + i * (chunk_width + chunk_gap);
        if (cx + chunk_width > inner.x + fill_w) {
            break;
        }
        painter.fill_rect({cx, inner.y, chunk_width, inner.height}, fill_c);
    }
}

void Win95Theme::draw_tab_content_background(Painter &painter, Rect const &rect) const {
    painter.fill_rect(rect, palette.window);
}

void Win95Theme::draw_list_item(Painter &painter, Rect const &rect, std::string_view text,
                                Icon const &icon, bool selected, bool hovered,
                                bool alternate) const {
    BaseTheme::draw_list_item(painter, rect, text, icon, selected, false, alternate);
}

void Win95Theme::draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                          WidgetState const &state, TabOrientation orientation, bool has_close,
                          bool hovered_close, float font_size) const {
    auto tab_rect = rect;
    if (!active) {
        switch (orientation) {
        case TabOrientation::North:
            tab_rect.y += 4.0f;
            tab_rect.height -= 4.0f;
            break;
        case TabOrientation::South:
            tab_rect.height -= 4.0f;
            break;
        case TabOrientation::West:
        case TabOrientation::WestVertical:
            tab_rect.x += 4.0f;
            tab_rect.width -= 4.0f;
            break;
        case TabOrientation::East:
        case TabOrientation::EastVertical:
            tab_rect.width -= 4.0f;
            break;
        }
    }

    auto bg = active ? palette.tab_select_background : palette.tab_background;
    painter.fill_rect(tab_rect, bg);

    BaseTheme::draw_tab(painter, tab_rect, text, active, state, orientation, has_close,
                        hovered_close, font_size);

    auto x = tab_rect.x, y = tab_rect.y, w = tab_rect.width, h = tab_rect.height;
    auto tl_outer = palette.light;
    auto tl_inner = Color::lerp(palette.light, palette.window, 0.3f);
    auto br_inner = palette.shadow;
    auto br_outer = palette.dark_shadow;
    auto draw_top = true;
    auto draw_bottom = true;
    auto draw_left = true;
    auto draw_right = true;

    if (active) {
        switch (orientation) {
        case TabOrientation::North:
            draw_bottom = false;
            break;
        case TabOrientation::South:
            draw_top = false;
            break;
        case TabOrientation::West:
        case TabOrientation::WestVertical:
            draw_right = false;
            break;
        case TabOrientation::East:
        case TabOrientation::EastVertical:
            draw_left = false;
            break;
        }
    }

    auto tx1 = draw_left ? x : x + 1;
    auto tx2 = draw_right ? x + w - 1 : x + w - 2;
    auto bx1 = draw_left ? x : x + 1;
    auto bx2 = draw_right ? x + w - 1 : x + w - 2;
    auto ly1 = draw_top ? y : y + 1;
    auto ly2 = draw_bottom ? y + h - 1 : y + h - 2;
    auto ry1 = draw_top ? y : y + 1;
    auto ry2 = draw_bottom ? y + h - 1 : y + h - 2;

    if (draw_top) {
        painter.draw_line({tx1, y}, {tx2, y}, tl_outer, 1.0f);
        painter.draw_line({tx1 + 1, y + 1}, {tx2 - 1, y + 1}, tl_inner, 1.0f);
    }
    if (draw_left) {
        painter.draw_line({x, ly1}, {x, ly2}, tl_outer, 1.0f);
        painter.draw_line({x + 1, ly1 + 1}, {x + 1, ly2 - 1}, tl_inner, 1.0f);
    }
    if (draw_bottom) {
        painter.draw_line({bx1, y + h - 1}, {bx2, y + h - 1}, br_outer, 1.0f);
        painter.draw_line({bx1 + 1, y + h - 2}, {bx2 - 1, y + h - 2}, br_inner, 1.0f);
    }
    if (draw_right) {
        painter.draw_line({x + w - 1, ry1}, {x + w - 1, ry2}, br_outer, 1.0f);
        painter.draw_line({x + w - 2, ry1 + 1}, {x + w - 2, ry2 - 1}, br_inner, 1.0f);
    }
}

} // namespace toolkit
