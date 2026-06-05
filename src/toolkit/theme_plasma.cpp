// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_plasma.hpp"
#include "toolkit/button.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include <memory>

namespace toolkit {

// FIXME: the macos title has a very similar button, merge them
class PlasmaButton : public Button {
  public:
    PlasmaButton(DecorationButton type, std::string tooltip) : Button(""), type_(type) {
        set_flat(true);
        set_tooltip(std::move(tooltip));
    }

    void paint(Painter &painter) override {
        auto interaction = ButtonState::Normal;
        if (is_pressed()) {
            interaction = ButtonState::ClickedInside;
        } else if (is_hovered()) {
            interaction = ButtonState::Hovered;
        }
        auto wstate = WidgetState{
            .interaction = interaction,
            .focused = is_focused(),
            .enabled = is_enabled(),
            .window_active = window_ ? window_->is_active() : true,
            .checked = false,
        };
        Theme::current().draw_window_button(painter, {0, 0, rect_.width, rect_.height}, type_,
                                            wstate);
    }

    Size size_hint() const override { return {20.0f, 20.0f}; }

  private:
    DecorationButton type_;
};

// FIXME: This class is very similar to the windows and macos titlebar
class PlasmaTitleBar : public Widget {
    HBoxLayout *layout = nullptr;
    Label *title_label = nullptr;
    Button *max_btn = nullptr;

  public:
    explicit PlasmaTitleBar(Window *window) : window_(window) {
        // set_on_top(true);
        layout = new HBoxLayout();
        layout->set_spacing(8.0f);

        auto *app_button = new PlasmaButton(DecorationButton::Menu, "Menu");

        auto *close_btn = new PlasmaButton(DecorationButton::Close, "Close");
        close_btn->on_click = [this] { window_->close(); };

        auto *min_btn = new PlasmaButton(DecorationButton::Minimize, "Minimize");
        min_btn->on_click = [this] { window_->minimize(); };

        max_btn = new PlasmaButton(DecorationButton::Maximize, "Zoom");
        max_btn->on_click = [this] {
            if (window_->is_maximized()) {
                window_->restore();
            } else {
                window_->maximize();
            }
        };

        auto m = layout->get_margins();
        m.right = 5.0;
        m.left = 5.0;
        layout->set_margins(m);
        layout->add_widget(std::unique_ptr<Widget>(app_button));
        title_label = new Label(std::string{window->title()});
        title_label->set_alignment(Alignment::Center).set_shrinkable(true).set_elide(true);
        layout->add_widget(std::unique_ptr<Label>(title_label), 1);
        layout->add_widget(std::unique_ptr<Widget>(min_btn));
        layout->add_widget(std::unique_ptr<Widget>(max_btn));
        layout->add_widget(std::unique_ptr<Widget>(close_btn));
    }

    void paint(Painter &painter) override {
        auto const &pal = Theme::current().palette;
        auto active = window_->is_active();
        // auto fg = active ? pal.highlighted_text : pal.text_disabled;
        // auto bg = active ? pal.window : pal.window_inactive.value_or(pal.window);
        auto fg = pal.text;
        auto bg = pal.window;
        if (!active && pal.window_inactive) {
            bg = pal.window_inactive.value();
        }

        painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);

        if (window_->is_maximized()) {
            max_btn->set_tooltip("Restore");
        } else {
            max_btn->set_tooltip("Maximize");
        }

        // FIXME: update window label only when the window title changed
        title_label->set_text(std::string(window_->title()));
        // FIXME: update color on blur/active
        title_label->set_color(fg);

        layout->paint(painter);
    }

    void set_rect(Rect const &rect) override {
        Widget::set_rect(rect);
        layout->set_rect(rect);
    }

    bool handle_mouse(MouseEvent const &event) override {
        if (layout->handle_mouse(event)) {
            return true;
        }
        if (!rect_.contains(event.position)) {
            return false;
        }
        if (event.type == MouseEvent::Type::Press) {
            if (event.click_count == 2) {
                if (window_->is_maximized()) {
                    window_->restore();
                } else {
                    window_->maximize();
                }
                return true;
            }
            window_->start_system_move(event.serial);
            return true;
        }
        return false;
    }

    void for_each_child(std::function<void(Widget *)> const &callback) override {
        layout->for_each_child(callback);
    }

    void on_theme_changed() override { set_rect(rect_); }

    Size size_hint() const override {
        auto const &m = Theme::current().palette.window_decoration;
        return {100.0f, m.top};
    }

  private:
    Window *window_;
};

Plasma6Theme::Plasma6Theme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Plasma 6";
    button.padding = {8, 16, 8, 16};
    slider.handle_size = 20.0f;
    slider.groove_thickness = 6.0f;
    focus_ring_margin = 2.0f;
    focus_ring_corner_radius = 4.0f;
    tab_widget.indicator_weight = -2.0f;

    scrollbar.show_buttons = false;
    scrollbar.thickness = 34.0f;
    scrollbar.show_frame = false;
    scrollbar.padding = {0, 4, 0, 4};
}

std::unique_ptr<Widget> Plasma6Theme::create_title_bar(Window *window) const {
    return std::make_unique<PlasmaTitleBar>(window);
}

Palette Plasma6Theme::default_palette(ColorScheme scheme) const {
    Palette p = BaseTheme::default_palette(scheme);
    Color plasma6_color = Color::from_rgb(0x3daee9);
    p.inline_scrollbars = false;
    p.corner_radius = 6.0f;
    p.tab_radius = 5.0f;
    p.window_decoration = {32, 0, 0, 0};
    p.bottom_shadow = true;
    p.chrome_lines = !true;

    switch (scheme) {
    case ColorScheme::Light:
        p.window_inactive = Color::from_rgb(0xeff0f1);
        p.window = Color::from_rgb(0xe3e5e7);
        p.base = Color::from_rgb(0xfcfcfc);
        p.alternate = Color::from_rgb(0xeff0f1);
        p.text = Color::from_rgb(0x232629);
        p.text_disabled = Color::from_argb(0xFF7F8C8D);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = plasma6_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_rgb(0xc7c8c9);
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
        p.tab_select_background = p.window;
        p.tab_background = Color::from_rgb(0xc7c8c9);
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
        p.tab_select_background = p.window;
        p.tab_background = p.window;
        break;
    }
    return p;
}

void Plasma6Theme::draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                  int depth, bool has_children, bool expanded, bool selected,
                                  bool hovered, bool alternate) const {
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

void Plasma6Theme::draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                                      WidgetState const &state) const {
    auto bg = palette.window;
    auto hovered = state.interaction == ButtonState::Hovered;
    auto pressed = state.interaction == ButtonState::ClickedInside;
    auto center = Point{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};

    if (hovered || pressed) {
        if (button == DecorationButton::Close) {
            // bg = Color::from_rgb(0xe74c3c);
            bg = palette.error;
        } else if (button != DecorationButton::Menu) {
            bg = palette.text;
        }
        if (pressed && button != DecorationButton::Menu) {
            bg = bg.lighten(0.1f);
        }
        if (button != DecorationButton::Menu) {
            auto cr = std::min(rect.width, rect.height) / 2.0f;
            painter.fill_circle(center, cr, bg);
        }
    }

    auto size = std::min(rect.width, rect.height) * 0.25f;
    auto fg = (hovered || pressed) ? palette.window : palette.text;

    if (button == DecorationButton::Minimize) {
        auto vd = size * 0.66f;
        painter.draw_line({center.x - size, center.y}, {center.x, center.y + vd}, fg, 1.5f);
        painter.draw_line({center.x, center.y + vd}, {center.x + size, center.y}, fg, 1.5f);
    } else if (button == DecorationButton::Maximize) {
        painter.draw_line({center.x, center.y - size}, {center.x + size, center.y}, fg, 1.5f);
        painter.draw_line({center.x + size, center.y}, {center.x, center.y + size}, fg, 1.5f);
        painter.draw_line({center.x, center.y + size}, {center.x - size, center.y}, fg, 1.5f);
        painter.draw_line({center.x - size, center.y}, {center.x, center.y - size}, fg, 1.5f);
    } else if (button == DecorationButton::Restore) {
        painter.draw_rect({center.x - size, center.y - size + 2, size * 1.5f, size * 1.5f}, fg,
                          1.5f);
        painter.draw_rect({center.x - size + 2, center.y - size, size * 1.5f, size * 1.5f}, fg,
                          1.5f);
    } else if (button == DecorationButton::Close) {
        painter.draw_line({center.x - size, center.y - size}, {center.x + size, center.y + size},
                          fg, 1.5f);
        painter.draw_line({center.x + size, center.y - size}, {center.x - size, center.y + size},
                          fg, 1.5f);
    } else if (button == DecorationButton::Menu) {
        // Simple menu icon
        painter.draw_line({center.x - size, center.y - 3}, {center.x + size, center.y - 3}, fg,
                          1.5f);
        painter.draw_line({center.x - size, center.y}, {center.x + size, center.y}, fg, 1.5f);
        painter.draw_line({center.x - size, center.y + 3}, {center.x + size, center.y + 3}, fg,
                          1.5f);
    }
}

} // namespace toolkit
