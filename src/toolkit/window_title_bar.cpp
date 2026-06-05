// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/window_title_bar.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include <memory>

namespace toolkit {

WindowTitleBar::WindowTitleBar(Window *window) : window_(window) {
    set_on_top(true);
    layout_ = new HBoxLayout();
    auto const &p = Theme::current().palette;

    {
        auto btn = create_btn(DecorationButton::Menu);
        btn->set_flat(true).set_text("=");
        layout_->add_widget(std::unique_ptr<Widget>(btn));
    }

    title_label_ = new Label(std::string(window_->title()));
    title_label_->set_alignment(Alignment::Center).set_shrinkable(true).set_elide(true);
    layout_->add_widget(std::unique_ptr<Widget>(title_label_), 1);

    {
        auto btn = create_btn(DecorationButton::Minimize);
        btn->set_flat(true).set_text("-").set_tooltip("Minimize");
        layout_->add_widget(std::unique_ptr<Button>(btn));
    }
    {
        max_btn_ = new Button("");
        max_btn_->on_click = [this] {
            if (window_->is_maximized()) {
                window_->restore();
            } else {
                window_->maximize();
            }
        };
        max_btn_->set_text(window_->is_maximized() ? "\u29C9" : "\u29C8").set_flat(true);
        layout_->add_widget(std::unique_ptr<Button>(max_btn_));
    }
    {
        auto btn = create_btn(DecorationButton::Close);
        btn->set_flat(true).set_text("x").set_tooltip("Close");
        layout_->add_widget(std::unique_ptr<Button>(btn));
    }
}

auto WindowTitleBar::create_btn(DecorationButton type) -> Button * {
    auto *btn = new Button("");
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

    painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);

    if (window_->is_maximized()) {
        max_btn_->set_text("[]").set_tooltip("Restore");
    } else {
        max_btn_->set_text("^").set_tooltip("Maximized");
    }

    // FIXME: update window label only when the window title changed
    title_label_->set_text(std::string(window_->title()));
    // FIXME: update color on blur/active
    title_label_->set_color(fg);

    layout_->paint(painter);
}

void WindowTitleBar::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    layout_->set_rect(rect);
}

bool WindowTitleBar::handle_mouse(MouseEvent const &event) {
    if (layout_->handle_mouse(event)) {
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
