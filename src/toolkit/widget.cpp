// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"

namespace toolkit {

bool Widget::debug_show_frames = false;

void Widget::invalidate_layout() {
    layout_dirty = true;
    if (window_) {
        window_->request_redraw();
    }
    if (parent_) {
        parent_->invalidate_layout();
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
