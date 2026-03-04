// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/slider.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

Slider::Slider(SliderOrientation orientation) : orientation_(orientation) {
    focusable_ = true;
}

void Slider::set_value(float v) {
    v = std::clamp(v, min_, max_);
    if (value_ == v) return;
    value_ = v;
    if (window_) window_->request_redraw();
}

void Slider::set_minimum(float m) {
    if (min_ == m) return;
    min_ = m;
    set_value(value_);
    if (window_) window_->request_redraw();
}

void Slider::set_maximum(float m) {
    if (max_ == m) return;
    max_ = m;
    set_value(value_);
    if (window_) window_->request_redraw();
}

void Slider::set_range(float min, float max) {
    min_ = min;
    max_ = max;
    set_value(value_);
    if (window_) window_->request_redraw();
}

float Slider::value_to_pos(float v) const {
    auto const &style = Theme::current().slider;
    float h_size = style.handle_size;
    bool horizontal = orientation_ == SliderOrientation::Horizontal;
    float length = horizontal ? rect_.width : rect_.height;
    
    if (max_ <= min_) return horizontal ? rect_.x + h_size / 2 : rect_.y + h_size / 2;
    
    float ratio = (v - min_) / (max_ - min_);
    float track_len = length - h_size;
    float offset = ratio * track_len + h_size / 2;
    
    if (horizontal) {
        return rect_.x + offset;
    } else {
        // Vertical slider: 0 is at bottom
        return rect_.y + length - offset;
    }
}

float Slider::pos_to_value(float p) const {
    auto const &style = Theme::current().slider;
    float h_size = style.handle_size;
    bool horizontal = orientation_ == SliderOrientation::Horizontal;
    float length = horizontal ? rect_.width : rect_.height;
    float track_len = length - h_size;
    
    if (track_len <= 0) return min_;
    
    float offset;
    if (horizontal) {
        offset = p - rect_.x - h_size / 2;
    } else {
        offset = rect_.y + length - p - h_size / 2;
    }
    
    float ratio = std::clamp(offset / track_len, 0.0f, 1.0f);
    return min_ + ratio * (max_ - min_);
}

void Slider::update_value_from_pos(Point p) {
    float v = pos_to_value(orientation_ == SliderOrientation::Horizontal ? p.x : p.y);
    set_value(v);
    if (on_change) {
        on_change(value_);
    }
}

void Slider::paint(Painter &painter) {
    auto const &style = Theme::current().slider;
    bool horizontal = orientation_ == SliderOrientation::Horizontal;
    
    float h_size = style.handle_size;
    float g_thick = style.groove_thickness;
    
    // Draw groove
    Rect groove_rect;
    if (horizontal) {
        groove_rect = {rect_.x + h_size/2, rect_.y + (rect_.height - g_thick)/2, rect_.width - h_size, g_thick};
    } else {
        groove_rect = {rect_.x + (rect_.width - g_thick)/2, rect_.y + h_size/2, g_thick, rect_.height - h_size};
    }
    painter.fill_rounded_rect(groove_rect, style.groove, g_thick/2);
    
    // Draw handle
    float hp = value_to_pos(value_);
    Rect handle_rect;
    if (horizontal) {
        handle_rect = {hp - h_size/2, rect_.y + (rect_.height - h_size)/2, h_size, h_size};
    } else {
        handle_rect = {rect_.x + (rect_.width - h_size)/2, hp - h_size/2, h_size, h_size};
    }
    
    painter.fill_rounded_rect(handle_rect, style.handle, style.corner_radius);
    painter.draw_rounded_rect(handle_rect, style.handle_border, style.corner_radius, style.border_width);
    
    if (focused_) {
        painter.draw_focus_ring(rect_, style.corner_radius);
    }
}

bool Slider::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Press) {
        if (hit_test(event.position)) {
            dragging_ = true;
            update_value_from_pos(event.position);
            if (window_) window_->set_focused_widget(this);
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

    float step = (max_ - min_) / 20.0f; // 5% step
    if (step <= 0) step = 1.0f;

    float next_val = value_;
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
            on_change(value_);
        }
        return true;
    }
    return false;
}

Size Slider::size_hint() const {
    auto const &style = Theme::current().slider;
    float s = style.handle_size + 4.0f;
    if (orientation_ == SliderOrientation::Horizontal) {
        return {100.0f, s};
    } else {
        return {s, 100.0f};
    }
}

} // namespace toolkit
