// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/slider.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

Slider::Slider(SliderOrientation orientation) : orientation_(orientation) { set_focusable(true); }

Slider &Slider::set_value(float v) {
    v = std::clamp(v, min_, max_);
    if (value_ == v) {
        return *this;
    }
    value_ = v;
    if (window_) {
        window_->request_redraw("slider change");
    }
    return *this;
}

Slider &Slider::set_minimum(float m) {
    if (min_ == m) {
        return *this;
    }
    min_ = m;
    set_value(value_);
    if (window_) {
        window_->request_redraw("slider change");
    }
    return *this;
}

Slider &Slider::set_maximum(float m) {
    if (max_ == m) {
        *this;
    }
    max_ = m;
    set_value(value_);
    if (window_) {
        window_->request_redraw("slider change");
    }
    return *this;
}

Slider &Slider::set_range(float min, float max) {
    min_ = min;
    max_ = max;
    set_value(value_);
    if (window_) {
        window_->request_redraw("slider change");
    }
    return *this;
}

float Slider::value_to_pos(float v) const {
    auto const &style = Theme::current().slider;
    auto h_size = style.handle_size;
    auto horizontal = orientation_ == SliderOrientation::Horizontal;
    auto length = horizontal ? rect_.width : rect_.height;

    if (max_ <= min_) {
        return h_size / 2;
    }

    auto ratio = (v - min_) / (max_ - min_);
    auto track_len = length - h_size;
    auto offset = ratio * track_len + h_size / 2;

    if (horizontal) {
        return offset;
    } else {
        // Vertical slider: 0 is at bottom (local height - offset)
        return length - offset;
    }
}

float Slider::pos_to_value(float p) const {
    auto const &style = Theme::current().slider;
    auto h_size = style.handle_size;
    auto horizontal = orientation_ == SliderOrientation::Horizontal;
    auto length = horizontal ? rect_.width : rect_.height;
    auto track_len = length - h_size;

    if (track_len <= 0) {
        return min_;
    }

    auto offset = 0.0f;
    if (horizontal) {
        offset = p - h_size / 2;
    } else {
        offset = length - p - h_size / 2;
    }

    auto ratio = std::clamp(offset / track_len, 0.0f, 1.0f);
    return min_ + ratio * (max_ - min_);
}

void Slider::update_value_from_pos(Point p) {
    auto v = pos_to_value(orientation_ == SliderOrientation::Horizontal ? p.x : p.y);
    set_value(v);
    if (on_change) {
        on_change(*this, value_);
    }
}

void Slider::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto horizontal = orientation_ == SliderOrientation::Horizontal;
    auto range = max_ - min_;
    auto normalized_value = (range > 0) ? (value_ - min_) / range : 0.0f;

    Theme::current().draw_slider(painter, rect, normalized_value, horizontal, false, false,
                                 is_focused(), is_enabled());
}

bool Slider::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Press) {
        if (Rect{0, 0, rect_.width, rect_.height}.contains(event.position)) {
            dragging_ = true;
            update_value_from_pos(event.position);
            if (window_) {
                window_->set_focused_widget(this);
            }
            return true;
        }
    } else if (event.type == MouseEvent::Type::Drag && dragging_) {
        update_value_from_pos(event.position);
        return true;
    } else if (event.type == MouseEvent::Type::Release && dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

bool Slider::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto step = (max_ - min_) / 20.0f; // 5% step
    if (step <= 0) {
        step = 1.0f;
    }

    auto next_val = value_;
    switch (event.key) {
    case Key::Left:
    case Key::Down:
        next_val -= step;
        break;
    case Key::Right:
    case Key::Up:
        next_val += step;
        break;
    case Key::Home:
        next_val = min_;
        break;
    case Key::End:
        next_val = max_;
        break;
    default:
        return false;
    }

    if (next_val != value_) {
        set_value(next_val);
        if (on_change) {
            on_change(*this, value_);
        }
        return true;
    }
    return false;
}

Size Slider::size_hint() const {
    auto const &style = Theme::current().slider;
    auto s = style.handle_size + 4.0f;
    if (orientation_ == SliderOrientation::Horizontal) {
        return {100.0f, s};
    } else {
        return {s, 100.0f};
    }
}

} // namespace toolkit
