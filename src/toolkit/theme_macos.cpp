// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_macos.hpp"
#include "toolkit/button.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include "toolkit/window_title_bar.hpp"
#include <memory>

namespace toolkit {

class MacOSTitleBar : public WindowTitleBar {
  public:
    MacOSTitleBar(Window *w) : WindowTitleBar(w) {}

    virtual void initializeTitleBar() override {
        layout = new HBoxLayout();
        layout->set_spacing(8.0f);

        auto p = layout->get_margins();
        p.left = 5.0f;
        layout->set_margins(p);

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

        layout->add_widget(std::unique_ptr<Widget>(close_btn));
        layout->add_widget(std::unique_ptr<Widget>(min_btn));
        layout->add_widget(std::unique_ptr<Widget>(max_btn));
        title_label = new Label(std::string{window_->title()});
        title_label->set_alignment(Alignment::Center).set_shrinkable(true).set_elide(true);
        layout->add_widget(std::unique_ptr<Label>(title_label), 1);
    }
};

MacOSTheme::MacOSTheme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "macOS";
    focus_ring_margin = 2.0f;
    focus_ring_corner_radius = 4.0f;
}

Palette MacOSTheme::default_palette(ColorScheme scheme) const {
    Palette p = BaseTheme::default_palette(scheme);
    p.window_decoration = {38, 0, 0, 0};
    auto macBlue = Color::from_argb(0xFF0A84FF);
    p.border_width = 0.5f;

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_argb(0xFFF2F2F7);
        p.window_inactive = Color::from_argb(0xFFF3F3F3);
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
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
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
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.tab_radius = 4.0f;
        break;
    }
    return p;
}

void MacOSTheme::draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                                    WidgetState const &state) const {
    auto center = Point{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
    auto r = std::min(rect.width, rect.height) * 0.25f;

    Color color;
    switch (button) {
    case DecorationButton::Close:
        color = Color::from_rgb(0xff5f57);
        break;
    case DecorationButton::Minimize:
        color = Color::from_rgb(0xffbd2e);
        break;
    case DecorationButton::Maximize:
    case DecorationButton::Restore:
        color = Color::from_rgb(0x28c840);
        break;
    case DecorationButton::Menu:
        color = palette.text;
        break;
    }

    if (state.interaction == ButtonState::ClickedInside) {
        color = color.darken(0.2f);
    } else if (state.interaction == ButtonState::Hovered) {
        color = color.lighten(0.1f);
    }

    if (button != DecorationButton::Menu) {
        painter.fill_circle(center, r, color);
        painter.draw_circle(center, r, color.darken(0.15f), 0.5f);
    }

    if (state.interaction == ButtonState::Hovered ||
        state.interaction == ButtonState::ClickedInside) {
        auto symbol_c = Color::rgba(0, 0, 0, 0.5f);
        auto s = r * 0.4f;
        if (button == DecorationButton::Close) {
            painter.draw_line({center.x - s, center.y - s}, {center.x + s, center.y + s}, symbol_c,
                              1.0f);
            painter.draw_line({center.x + s, center.y - s}, {center.x - s, center.y + s}, symbol_c,
                              1.0f);
        } else if (button == DecorationButton::Minimize) {
            painter.draw_line({center.x - s, center.y}, {center.x + s, center.y}, symbol_c, 1.5f);
        } else if (button == DecorationButton::Maximize || button == DecorationButton::Restore) {
            painter.draw_rect({center.x - s, center.y - s, s * 2, s * 2}, symbol_c, 1.0f);
        }
    }
}

std::unique_ptr<Widget> MacOSTheme::create_title_bar(Window *window) const {
    auto b = std::make_unique<MacOSTitleBar>(window);
    b->initializeTitleBar();
    return b;
}

void MacOSTheme::draw_tab_content_background(Painter &painter, Rect const &rect) const {
    painter.fill_rect(rect, palette.window);
}

} // namespace toolkit
