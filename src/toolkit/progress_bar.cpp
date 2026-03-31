#include "toolkit/progress_bar.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

ProgressBar::ProgressBar() {}

void ProgressBar::set_value(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    if (v == value_) {
        return;
    }
    value_ = v;
    if (window_ && is_effectively_visible()) {
        window_->request_redraw("progress change");
    }
}

void ProgressBar::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    Theme::current().draw_progress_bar(painter, rect, value_, is_enabled());
}

Size ProgressBar::size_hint() const {
    auto const h = Theme::current().palette.progress_bar_height;
    return {0, h};
}

} // namespace toolkit
