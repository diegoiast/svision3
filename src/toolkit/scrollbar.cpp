// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/scrollbar.hpp"
#include "toolkit/events.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/types.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

Scrollbar::Scrollbar(Orientation o) : orientation_(o) { state.focusable = true; }

auto Scrollbar::size_hint() const -> Size {
    auto const &style = Theme::current().style.scrollbar;
    if (orientation_ == Orientation::Horizontal) {
        return {100, style.thickness};
    } else {
        return {style.thickness, 100};
    }
}

auto Scrollbar::set_value(float v) -> Scrollbar & {
    v = std::clamp(v, min_, max_);
    if (value_ == v) {
        return *this;
    }
    value_ = v;
    if (on_change) {
        on_change(value_);
    }
    return *this;
}

auto Scrollbar::set_range(float min, float max) -> Scrollbar & {
    min_ = min;
    max_ = max;
    if (value_ < min_) {
        set_value(min_);
    } else if (value_ > max_) {
        set_value(max_);
    }
    return *this;
}

auto Scrollbar::normalized_value() const -> float {
    auto range = max_ - min_;
    if (range <= 0.0f) {
        return 0.0f;
    }
    return (value_ - min_) / range;
}

void Scrollbar::set_normalized_value(float v) { set_value(min_ + v * (max_ - min_)); }

void Scrollbar::step_by(float delta) { set_value(value_ + delta); }

void Scrollbar::page_by(float delta) { set_value(value_ + (delta > 0 ? step_page_ : -step_page_)); }

void Scrollbar::go_to_min() { set_value(min_); }

void Scrollbar::go_to_max() { set_value(max_); }

void Scrollbar::stop_auto_repeat() {
    if (auto_repeat_timer_id_ && window_) {
        window_->stop_timer(auto_repeat_timer_id_);
        auto_repeat_timer_id_ = 0;
    }
}

void Scrollbar::start_auto_repeat(float direction) {
    stop_auto_repeat();
    if (!window_) {
        return;
    }
    auto const &p = Theme::current().palette;
    auto_repeat_timer_id_ = window_->start_timer(
        p.auto_repeat_delay,
        [this, direction] {
            if (!pressed_left_ && !pressed_right_) {
                stop_auto_repeat();
                return;
            }
            step_by(direction * step_small_);
            stop_auto_repeat();
            if (window_) {
                auto_repeat_timer_id_ = window_->start_timer(
                    Theme::current().palette.auto_repeat_interval,
                    [this, direction] {
                        if (!pressed_left_ && !pressed_right_) {
                            stop_auto_repeat();
                            return;
                        }
                        step_by(direction * step_small_);
                    },
                    true);
            }
        },
        false);
}

auto Scrollbar::button_size() const -> float {
    auto const &style = Theme::current().style.scrollbar;
    if (!style.show_buttons) {
        return 0.0f;
    }
    float thickness = orientation_ == Orientation::Horizontal ? rect_.height : rect_.width;
    thickness = std::min(thickness, style.thickness);
    return std::min(style.button_size, thickness);
}

auto Scrollbar::left_button_rect() const -> Rect {
    auto const &style = Theme::current().style.scrollbar;
    auto bs = button_size();
    float thickness = orientation_ == Orientation::Horizontal ? rect_.height : rect_.width;
    thickness = std::min(thickness, style.thickness);
    float center_off =
        ((orientation_ == Orientation::Horizontal ? rect_.height : rect_.width) - thickness) / 2.0f;

    if (orientation_ == Orientation::Horizontal) {
        return {0, center_off + (thickness - bs) / 2.0f, bs, bs};
    } else {
        return {center_off + (thickness - bs) / 2.0f, 0, bs, bs};
    }
}

auto Scrollbar::right_button_rect() const -> Rect {
    auto const &style = Theme::current().style.scrollbar;
    auto bs = button_size();
    float thickness = orientation_ == Orientation::Horizontal ? rect_.height : rect_.width;
    thickness = std::min(thickness, style.thickness);
    float center_off =
        ((orientation_ == Orientation::Horizontal ? rect_.height : rect_.width) - thickness) / 2.0f;

    if (orientation_ == Orientation::Horizontal) {
        return {rect_.width - bs, center_off + (thickness - bs) / 2.0f, bs, bs};
    } else {
        return {center_off + (thickness - bs) / 2.0f, rect_.height - bs, bs, bs};
    }
}

auto Scrollbar::track_rect() const -> Rect {
    auto const &style = Theme::current().style.scrollbar;
    auto bs = button_size();
    float thickness = orientation_ == Orientation::Horizontal ? rect_.height : rect_.width;
    thickness = std::min(thickness, style.thickness);
    float center_off =
        ((orientation_ == Orientation::Horizontal ? rect_.height : rect_.width) - thickness) / 2.0f;

    if (orientation_ == Orientation::Horizontal) {
        auto x = bs;
        auto w = rect_.width - 2 * bs;
        return {x, center_off, w, thickness};
    } else {
        auto y = bs;
        auto h = rect_.height - 2 * bs;
        return {center_off, y, thickness, h};
    }
}

auto Scrollbar::thumb_rect() const -> Rect {
    auto const &style = Theme::current().style.scrollbar;
    auto bs = button_size();
    auto nv = normalized_value();
    float thickness = orientation_ == Orientation::Horizontal ? rect_.height : rect_.width;
    thickness = std::min(thickness, style.thickness);
    float center_off =
        ((orientation_ == Orientation::Horizontal ? rect_.height : rect_.width) - thickness) / 2.0f;

    if (orientation_ == Orientation::Horizontal) {
        auto track_w = rect_.width - 2 * bs;
        auto thumb_w = std::max(kMinThumbSize, track_w * 0.1f);
        auto max_thumb_x = track_w - thumb_w;
        auto thumb_x = bs + nv * max_thumb_x;
        auto thumb_h = std::max(1.0f, thickness - style.padding.top - style.padding.bottom);
        return {thumb_x, center_off + (thickness - thumb_h) / 2.0f, thumb_w, thumb_h};
    } else {
        auto track_h = rect_.height - 2 * bs;
        auto thumb_h = std::max(kMinThumbSize, track_h * 0.1f);
        auto max_thumb_y = track_h - thumb_h;
        auto thumb_y = bs + nv * max_thumb_y;
        auto thumb_w = std::max(1.0f, thickness - style.padding.left - style.padding.right);
        return {center_off + (thickness - thumb_w) / 2.0f, thumb_y, thumb_w, thumb_h};
    }
}

auto Scrollbar::hit_left_button(Point p) const -> bool { return left_button_rect().contains(p); }

auto Scrollbar::hit_right_button(Point p) const -> bool { return right_button_rect().contains(p); }

auto Scrollbar::hit_thumb(Point p) const -> bool { return thumb_rect().contains(p); }

void Scrollbar::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };

    Theme::current().draw_scrollbar(painter, rect, normalized_value(), orientation_, wstate,
                                    hovered_left_, pressed_left_, hovered_right_, pressed_right_,
                                    hovered_thumb_);
}

bool Scrollbar::handle_mouse(MouseEvent const &event) {
    auto horizontal = orientation_ == Orientation::Horizontal;
    if (event.type == MouseEvent::Type::Scroll) {
        if (hit_test(event.position)) {
            auto delta = horizontal ? event.scroll_dx : -event.scroll_dy;
            step_by(delta * step_small_);
            return true;
        }
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (hit_left_button(event.position)) {
            pressed_left_ = true;
            step_by(-step_small_);
            start_auto_repeat(-1);
            return true;
        }
        if (hit_right_button(event.position)) {
            pressed_right_ = true;
            step_by(step_small_);
            start_auto_repeat(1);
            return true;
        }
        if (hit_thumb(event.position)) {
            dragging_ = true;
            drag_start_mouse_ = horizontal ? event.position.x : event.position.y;
            drag_start_value_ = value_;
            if (window_) {
                window_->grab_pointer();
            }
            return true;
        }
        // Click on track
        auto thumb = thumb_rect();
        auto pos = horizontal ? event.position.x : event.position.y;
        auto thumb_pos = horizontal ? thumb.x : thumb.y;
        if (pos < thumb_pos) {
            page_by(-step_page_);
        } else {
            page_by(step_page_);
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        if (dragging_ && window_) {
            window_->ungrab_pointer();
        }
        pressed_left_ = false;
        pressed_right_ = false;
        dragging_ = false;
        hovered_left_ = false;
        hovered_right_ = false;
        hovered_thumb_ = false;
        stop_auto_repeat();
        return true;
    }

    if (event.type == MouseEvent::Type::Drag) {
        if (dragging_) {
            auto bs = button_size();
            auto track_len = (horizontal ? rect_.width : rect_.height) - 2 * bs;
            auto thumb_len = (horizontal ? thumb_rect().width : thumb_rect().height);
            auto max_thumb_pos = track_len - thumb_len;
            if (max_thumb_pos > 0) {
                auto mouse_pos = horizontal ? event.position.x : event.position.y;
                auto mouse_delta = mouse_pos - drag_start_mouse_;
                auto nv = (drag_start_value_ - min_) / (max_ - min_);
                nv += mouse_delta / max_thumb_pos;
                set_normalized_value(nv);
            }
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Move) {
        if (dragging_) {
            dragging_ = false;
            stop_auto_repeat();
        }
        hovered_left_ = hit_left_button(event.position);
        hovered_right_ = hit_right_button(event.position);
        hovered_thumb_ = hit_thumb(event.position);
        return true;
    }

    if (event.type == MouseEvent::Type::Leave) {
        hovered_left_ = false;
        hovered_right_ = false;
        if (!dragging_) {
            hovered_thumb_ = false;
        }
        stop_auto_repeat();
        return true;
    }

    return false;
}

bool Scrollbar::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto horizontal = orientation_ == Orientation::Horizontal;
    auto prev_key = horizontal ? Key::Left : Key::Up;
    auto next_key = horizontal ? Key::Right : Key::Down;

    if (event.key == prev_key) {
        step_by(-step_small_);
        return true;
    }
    if (event.key == next_key) {
        step_by(step_small_);
        return true;
    }

    switch (event.key) {
    case Key::Home:
        go_to_min();
        return true;
    case Key::End:
        go_to_max();
        return true;
    case Key::PageUp:
        step_by(-step_page_);
        return true;
    case Key::PageDown:
        step_by(step_page_);
        return true;
    default:
        break;
    }

    if (event.ctrl) {
        // Fallback or secondary keys
        auto fallback_prev = horizontal ? Key::Up : Key::Left;
        auto fallback_next = horizontal ? Key::Down : Key::Right;
        if (event.key == fallback_prev) {
            step_by(-step_small_);
            return true;
        }
        if (event.key == fallback_next) {
            step_by(step_small_);
            return true;
        }
    }

    return false;
}

} // namespace toolkit
