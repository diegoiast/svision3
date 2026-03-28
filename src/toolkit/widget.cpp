// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include <spdlog/spdlog.h>

namespace toolkit {

bool Widget::debug_show_frames = false;

void Widget::invalidate_layout() {
    state.layout_dirty = true;
    spdlog::trace("Widget layout invalidated: {}",
                  state.tooltip.empty() ? "anonymous" : state.tooltip);
    if (window_) {
        window_->request_redraw("layout");
    }
    if (parent_) {
        parent_->invalidate_layout();
    }
}

Widget &Widget::set_layout_dirty(bool dirty)
{
    state.layout_dirty = dirty;
    return  *this;
}

Widget &Widget::set_focusable(bool f)
{
    state.focusable = f;
    return *this;
}

bool Widget::is_effectively_visible() const {
    if (!is_visible()) {
        return false;
    }
    if (parent_) {
        return parent_->is_effectively_visible();
    }
    return true;
}

void Widget::on_theme_changed() { invalidate_layout(); }

Widget &Widget::set_enabled(bool e) {
    if (state.enabled == e) {
        return *this;
    }
    state.enabled = e;
    if (window_) {
        window_->request_redraw("property change (enabled)");
    }
    return *this;
}

void Widget::set_focused(bool focused) {
    if (focused == state.focused) {
        return *this;
    }
    state.focused = focused;
    if (window_) {
        window_->request_redraw("proper change (focused)");
    }
    return *this;
}

Widget &Widget::set_visible(bool v) {
    if (is_visible() == v) {
        return *this;
    }
    state.visible = v;
    if (window_) {
        window_->request_redraw("property change (visible)");
    }
    return *this;
}

void Widget::set_tooltip(std::string text) {
    if (state.tooltip == text) {
        return;
    }
    state.tooltip = std::move(text);
    if (window_ && is_effectively_visible()) {
        window_->request_redraw("property change (tooltip)");
    }
}

auto Widget::dispatch_mouse_event(Widget *w, MouseEvent const &event) -> bool {
    auto local_ev = event;
    local_ev.position.x -= w->rect().x;
    local_ev.position.y -= w->rect().y;
    return w->handle_mouse(local_ev);
}

auto Widget::map_to_window(Point p) const -> Point {
    auto r = Point{p.x + rect_.x, p.y + rect_.y};
    if (parent_) {
        return parent_->map_to_window(r);
    }
    return r;
}

auto Widget::map_from_window(Point p) const -> Point {
    auto r = p;
    if (parent_) {
        r = parent_->map_from_window(p);
    }
    return {r.x - rect_.x, r.y - rect_.y};
}

void Widget::draw(Painter &painter) {
    if (!is_visible()) {
        return;
    }
    painter.push_clip(rect_);
    painter.push_translation({rect_.x, rect_.y});
    if (background_color_) {
        painter.fill_rect({0, 0, rect_.width, rect_.height}, *background_color_);
    }
    paint(painter);
    painter.pop_translation();
    painter.pop_clip();
}

bool Widget::handle_key_impl(KeyEvent const &event) {
    for (auto const &cmd : commands_) {
        if (cmd->matches_key_event(event)) {
            cmd->execute();
            return true;
        }
    }

    if (state.focused || state.non_focus_input) {
        return handle_key(event);
    }

    return false;
}

} // namespace toolkit
