// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/window_title_bar.hpp"
#include "toolkit/image_widget.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"

#include <memory>
#include <spdlog/spdlog.h>

namespace toolkit {

TitlebarButton::TitlebarButton(DecorationButton type, std::string tooltip, Size size_hint)
    : Button(""), type_(type), custom_size_hint(size_hint) {
    set_flat(true);
    set_tooltip(std::move(tooltip));
}

void TitlebarButton::paint(Painter &painter) {
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
    Theme::current().draw_window_button(painter, {0, 0, rect_.width, rect_.height}, type_, wstate);
}

TitleBarIcon::TitleBarIcon(Window *w) : window_(w) {
    set_focusable(false);
    set_show_checkerboard(false);
}

bool TitleBarIcon::handle_mouse(MouseEvent const &event) {
    if (!hit_test(event.position)) {
        return false;
    }
    if (event.type == MouseEvent::Type::Press) {
        auto shadow = (window_->options().csd && !window_->is_maximized())
                          ? Theme::current().style.shadow.size
                          : 0.0f;
        auto menu_pos = map_to_window({0, rect_.height});
        menu_pos.x += shadow;
        menu_pos.y += shadow;
        window_->platform_window()->show_system_menu(menu_pos);
        return true;
    }
    return false;
}

WindowTitleBar::WindowTitleBar(Window *w) { set_window(w); }

void WindowTitleBar::initializeTitleBar() {
    layout = new HBoxLayout();
    layout->set_window(window_);
    layout->set_margins(Margins{8.0f, 12.0, 8.0, 12.0f});
    layout->set_spacing(8.0f);

    icon_widget = new TitleBarIcon(window_);
    icon_widget->set_min_size({16, 16});
    icon_widget->set_max_size({16, 16});
    icon_widget->set_image(window_->get_icon());

    close_btn = new TitlebarButton(DecorationButton::Close, "Close");
    close_btn->on_click = [this] { window_->close(); };

    min_btn = new TitlebarButton(DecorationButton::Minimize, "Minimize");
    min_btn->on_click = [this] { window_->minimize(); };

    max_btn = new TitlebarButton(DecorationButton::Maximize, "Zoom");
    max_btn->on_click = [this] {
        if (window_->is_maximized()) {
            window_->restore();
        } else {
            window_->maximize();
        }
    };

    sync_button_states();

    layout->add_widget(std::unique_ptr<Widget>(icon_widget));
    title_label = new Label(std::string{window_->title()});
    title_label->set_alignment(Alignment::Center).set_shrinkable(true).set_elide(true);
    layout->add_widget(std::unique_ptr<Label>(title_label), 1);
    layout->add_widget(std::unique_ptr<Widget>(min_btn));
    layout->add_widget(std::unique_ptr<Widget>(max_btn));
    layout->add_widget(std::unique_ptr<Widget>(close_btn));
}

auto WindowTitleBar::create_btn(DecorationButton type) -> Button * {
    auto tooltip = std::string("");

    switch (type) {
    case DecorationButton::Close:
        tooltip = "Close";
        break;
    case DecorationButton::Minimize:
        tooltip = "Minimize";
        break;
    case DecorationButton::Maximize:
        tooltip = "Maximize";
        break;
    case DecorationButton::Restore:
        tooltip = "Restore";
        break;
    case DecorationButton::Menu:
        tooltip = "Menu";
        break;
    }

    auto *btn = new TitlebarButton(type, tooltip);
    btn->on_click = [this, type, btn] {
        switch (type) {
        case DecorationButton::Close:
            window_->close();
            break;
        case DecorationButton::Minimize:
            window_->minimize();
            break;
        case DecorationButton::Maximize:
            window_->maximize();
            break;
        case DecorationButton::Restore:
            window_->restore();
            break;
        case DecorationButton::Menu: {
            auto menu_pos = map_to_window({btn->rect().x, btn->rect().y + btn->rect().height});
            window_->platform_window()->show_system_menu(menu_pos);
            break;
        }
        }
    };
    return btn;
}

void WindowTitleBar::sync_button_states() {
    if (min_btn) {
        min_btn->set_enabled(window_->is_minimizable());
    }
    if (max_btn) {
        max_btn->set_enabled(window_->is_maximizable());
        max_btn->set_tooltip(window_->is_maximized() ? "Restore" : "Maximize");
    }
    if (close_btn) {
        close_btn->set_enabled(window_->is_closable());
    }
}

Size WindowTitleBar::size_hint() const {
    auto const &m = Theme::current().Theme::current().style.window_decoration;
    return {100.0f, m.top};
}

void WindowTitleBar::paint(Painter &painter) {
    auto const &pal = Theme::current().palette;
    auto active = window_->is_active();
    auto bg = active ? pal.accent : pal.window;
    auto fg = active ? pal.highlighted_text : pal.text_disabled;

    // FIXME: it would be nice for this to be changable from other themes
    painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);
    sync_button_states();

    // FIXME: update window label only when the window title changed
    title_label->set_text(std::string(window_->title()));
    // FIXME: update color on blur/active
    title_label->set_color(fg);
    layout->paint(painter);
}

void WindowTitleBar::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    layout->set_rect(rect);
}

void WindowTitleBar::set_icon(Icon const &icon) {
    if (icon_widget) {
        icon_widget->set_image(icon);
    } else {
        spdlog::warn("WindowTitleBar::set_icon called but icon_widget is null!");
    }
}

bool WindowTitleBar::handle_mouse(MouseEvent const &event) {
    if (layout->handle_mouse(event)) {
        return true;
    }

    auto local_rect = Rect{0, 0, rect_.width, rect_.height};
    if (!local_rect.contains(event.position)) {
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (event.button == 1) { // Right click
            auto menu_pos = map_to_window(event.position);
            window_->platform_window()->show_system_menu(menu_pos);
            return true;
        }
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

} // namespace toolkit
