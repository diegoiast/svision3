#include "toolkit/progress_bar.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>

namespace toolkit {

ProgressBar::ProgressBar() {
    relative_coords_ = true;
}

void ProgressBar::set_value(float v) {
    value_ = std::clamp(v, 0.0f, 1.0f);
}

void ProgressBar::paint(Painter &painter) {
    auto const &style = Theme::current().progress_bar;
    float h = style.bar_height;
    float y = (rect_.height - h) / 2.0f;
    Rect trough{0, y, rect_.width, h};

    if (style.beveled) {
        painter.draw_frame(trough, style.background, style.border, style, true);
    } else {
        painter.fill_rounded_rect(trough, style.background, style.corner_radius);
        if (style.border_width > 0)
            painter.draw_rounded_rect(trough, style.border, style.corner_radius, style.border_width);
    }

    if (value_ > 0.0f) {
        float inset = style.beveled ? 2.0f : style.border_width;
        float inner_w = trough.width - inset * 2;
        float inner_h = trough.height - inset * 2;
        float fill_w = inner_w * value_;

        if (fill_w > 0.5f) {
            float fx = trough.x + inset;
            float fy = trough.y + inset;

            if (style.chunked) {
                // translate clip rect relative to our (0,0)
                painter.push_clip({fx, fy, fill_w, inner_h});
                float x = fx;
                while (x < fx + fill_w) {
                    float cw = std::min(style.chunk_width, fx + inner_w - x);
                    painter.fill_rect({x, fy, cw, inner_h}, style.fill);
                    x += style.chunk_width + style.chunk_gap;
                }
                painter.pop_clip();
            } else {
                Rect fill_rect{fx, fy, fill_w, inner_h};
                float r = std::max(0.0f, style.corner_radius - inset);
                painter.fill_rounded_rect(fill_rect, style.fill, r);
            }
        }
    }
}

Size ProgressBar::size_hint() const {
    auto const &style = Theme::current().progress_bar;
    return {0, style.bar_height + 8.0f};
}

} // namespace toolkit
