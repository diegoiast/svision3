// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include <spdlog/spdlog.h>

namespace toolkit {

bool Widget::debug_show_frames = false;

void Widget::invalidate_layout() {
    layout_dirty = true;
    spdlog::trace("Widget layout invalidated: {}", tooltip_.empty() ? "anonymous" : tooltip_);
    if (window_) {
        window_->request_redraw("layout");
    }
    if (parent_) {
        parent_->invalidate_layout();
    }
}

bool Widget::is_effectively_visible() const {
    if (!visible_) {
        return false;
    }
    if (parent_) {
        return parent_->is_effectively_visible();
    }
    return true;
}

void Widget::on_theme_changed() {
    invalidate_layout();
}

void Widget::set_enabled(bool e) {
    if (enabled_ == e) return;
    enabled_ = e;
    if (window_) {
        window_->request_redraw("property change (enabled)");
    }
}

void Widget::set_visible(bool v) {
    if (visible_ == v) return;
    visible_ = v;
    if (window_) {
        window_->request_redraw("property change (visible)");
    }
}

void Widget::set_tooltip(std::string text) {
    if (tooltip_ == text) return;
    tooltip_ = std::move(text);
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
    if (parent_) {
        return parent_->map_to_window({p.x + rect_.x, p.y + rect_.y});
    }
    return {p.x + rect_.x, p.y + rect_.y};
}

void Widget::draw(Painter &painter) {
    if (!visible_) {
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

} // namespace toolkit
