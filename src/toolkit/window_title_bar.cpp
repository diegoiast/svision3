// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/window_title_bar.hpp"
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

WindowTitleBar::WindowTitleBar(Window *w) {
    set_window(w);
}

void WindowTitleBar::initializeTitleBar() {
    layout = new HBoxLayout();
    layout->set_spacing(8.0f);

    auto *app_button = new TitlebarButton(DecorationButton::Menu, "Menu");

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

    auto m = layout->get_margins();
    m.right = 5.0;
    m.left = 5.0;
    layout->set_margins(m);
    layout->add_widget(std::unique_ptr<Widget>(app_button));
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
    }

    auto *btn = new TitlebarButton(type, tooltip);
    btn->on_click = [this, type] {
        if (type == DecorationButton::Close) {
            window_->close();
        } else if (type == DecorationButton::Minimize) {
            window_->minimize();
        } else if (type == DecorationButton::Maximize) {
            window_->maximize();
        } else if (type == DecorationButton::Restore) {
            window_->restore();
        }
    };
    return btn;
}

Size WindowTitleBar::size_hint() const {
    auto const &m = Theme::current().palette.window_decoration;
    return {100.0f, m.top};
}

void WindowTitleBar::paint(Painter &painter) {
    auto const &pal = Theme::current().palette;
    auto active = window_->is_active();
    auto bg = active ? pal.accent : pal.window;
    auto fg = active ? pal.highlighted_text : pal.text_disabled;

    // FIXME: it would be nice for this to be changable from other themes
    painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);
    // FIXME: update window button tooltips on resize
    if (window_->is_maximized()) {
        max_btn->set_tooltip("Restore");
    } else {
        max_btn->set_tooltip("Maximized");
    }
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

bool WindowTitleBar::handle_mouse(MouseEvent const &event) {
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

} // namespace toolkit
