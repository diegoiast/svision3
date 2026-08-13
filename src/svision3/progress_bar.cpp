#include "svision3/progress_bar.hpp"
#include "svision3/button_state.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace svision3 {

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
    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };
    Theme::current().draw_progress_bar(painter, rect, value_, wstate);
}

Size ProgressBar::size_hint() const {
    auto const h = Theme::current().style.progressBar.height;
    return {0, h};
}

nlohmann::json ProgressBar::to_json() const {
    auto j = Widget::to_json();
    j["value"] = value_;
    return j;
}
} // namespace svision3
