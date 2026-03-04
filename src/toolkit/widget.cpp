// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/widget.hpp"

namespace toolkit {

bool Widget::debug_show_frames = false;

void Widget::draw(Painter &painter) {
    if (!visible_) {
        return;
    }
    painter.push_clip(rect_);
    if (background_color_) {
        painter.fill_rect(rect_, *background_color_);
    }
    paint(painter);
    painter.pop_clip();
}

} // namespace toolkit