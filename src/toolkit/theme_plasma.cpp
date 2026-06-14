// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_plasma.hpp"
#include "toolkit/button.hpp"
#include "toolkit/image_widget.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include "toolkit/window_title_bar.hpp"
#include <memory>

namespace toolkit {

class PlasmaTitleBar : public WindowTitleBar {
  public:
    using WindowTitleBar::WindowTitleBar;

    void initializeTitleBar() override {
        layout = new HBoxLayout();
        layout->set_window(window_);
        layout->set_spacing(8.0f);
        layout->set_margins({8, 12, 8, 12.0f});

        icon_widget = new TitleBarIcon(window_);
        icon_widget->set_window(window_);
        icon_widget->set_min_size({16, 16});
        icon_widget->set_max_size({16, 16});
        icon_widget->set_image(window_->get_icon());

        auto *close_btn = new TitlebarButton(DecorationButton::Close, "Close");
        close_btn->on_click = [this] { window_->close(); };

        auto *min_btn = new TitlebarButton(DecorationButton::Minimize, "Minimize");
        min_btn->on_click = [this] { window_->minimize(); };

        max_btn = new TitlebarButton(DecorationButton::Maximize, "Zoom");
        max_btn->on_click = [this] {
            if (window_->is_maximized()) {
                window_->restore();
            } else {
                window_->maximize();
            }
        };

        layout->add_widget(std::unique_ptr<Widget>(icon_widget));
        title_label = new Label(std::string{window_->title()});
        title_label->set_alignment(Alignment::Center).set_shrinkable(true).set_elide(true);
        layout->add_widget(std::unique_ptr<Label>(title_label), 1);
        layout->add_widget(std::unique_ptr<Widget>(min_btn));
        layout->add_widget(std::unique_ptr<Widget>(max_btn));
        layout->add_widget(std::unique_ptr<Widget>(close_btn));
    }

    void paint(Painter &painter) override {
        auto const &pal = Theme::current().palette;
        auto active = window_->is_active();

        // FIXME: only reason to override this method is to modify the background
        auto fg = pal.text;
        auto bg = pal.window;
        if (!active && pal.window_inactive) {
            bg = pal.window_inactive.value();
        }

        painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);
        if (window_->is_maximized()) {
            max_btn->set_tooltip("Restore");
        } else {
            max_btn->set_tooltip("Maximized");
        }
        title_label->set_text(std::string(window_->title()));
        title_label->set_color(fg);
        layout->paint(painter);
    }
};

Plasma6Theme::Plasma6Theme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Plasma 6";
    style.button.padding = {8, 16, 8, 16};
    style.slider.handle_size = 20.0f;
    style.slider.groove_thickness = 6.0f;
    style.ringFocus.margin = 2.0f;
    style.ringFocus.corner_radius = 4.0f;
    style.tabWidget.indicator_weight = -2.0f;

    style.scrollbar.show_buttons = false;
    style.scrollbar.thickness = 34.0f;
    style.scrollbar.show_frame = false;
    style.scrollbar.padding = {0, 4, 0, 4};

    style.inline_scrollbars = false;
    style.corner_radius = 6.0f;
    style.tabWidget.tab_radius = 5.0f;
    style.window_decoration = {32, 0, 0, 0};
    style.bottom_shadow = true;
    style.chrome_lines = false;
}

std::unique_ptr<Widget> Plasma6Theme::create_title_bar(Window *window) const {
    auto p = std::make_unique<PlasmaTitleBar>(window);
    p->initializeTitleBar();
    return p;
}
Palette Plasma6Theme::default_palette(ColorScheme scheme) const {
    Palette p = BaseTheme::default_palette(scheme);
    Color plasma6_color = Color::from_rgb(0x3daee9);

    switch (scheme) {
        // ...

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
    auto const &tree_style = this->style.treeView;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto indent = tree_style.indent;

    auto x_offset = tree_style.item_padding_h + depth * indent;

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

    auto size = 5.0f;
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
