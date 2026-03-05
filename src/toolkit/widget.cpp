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

void Widget::draw(Painter &painter) {
    if (!visible_) {
        return;
    }
    painter.push_clip(rect_);
    
    if (relative_coords_) {
        painter.push_translation({rect_.x, rect_.y});
        if (background_color_) {
            painter.fill_rect({0, 0, rect_.width, rect_.height}, *background_color_);
        }
        paint(painter);
        painter.pop_translation();
    } else {
        if (background_color_) {
            painter.fill_rect(rect_, *background_color_);
        }
        paint(painter);
    }
    
    painter.pop_clip();
}

} // namespace toolkit