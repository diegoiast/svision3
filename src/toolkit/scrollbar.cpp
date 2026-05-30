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

Scrollbar::Scrollbar() { state.focusable = true; }

auto Scrollbar::size_hint() const -> Size { return {100, kButtonSize + 4}; }

auto Scrollbar::set_value(float v) -> Scrollbar & {
    value_ = std::clamp(v, min_, max_);
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

auto Scrollbar::button_size() const -> float { return std::min(kButtonSize, rect_.height); }

auto Scrollbar::left_button_rect() const -> Rect {
    auto bs = button_size();
    return {0, (rect_.height - bs) / 2.0f, bs, bs};
}

auto Scrollbar::right_button_rect() const -> Rect {
    auto bs = button_size();
    return {rect_.width - bs, (rect_.height - bs) / 2.0f, bs, bs};
}

auto Scrollbar::track_rect() const -> Rect {
    auto bs = button_size();
    auto x = bs;
    auto w = rect_.width - 2 * bs;
    return {x, 0, w, rect_.height};
}

auto Scrollbar::thumb_rect() const -> Rect {
    auto bs = button_size();
    auto track_w = rect_.width - 2 * bs;
    auto nv = normalized_value();
    auto thumb_w = std::max(kMinThumbSize, track_w * 0.1f);
    auto max_thumb_x = track_w - thumb_w;
    auto thumb_x = bs + nv * max_thumb_x;
    auto thumb_h = rect_.height - 4.0f;
    return {thumb_x, (rect_.height - thumb_h) / 2.0f, thumb_w, thumb_h};
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

    Theme::current().draw_scrollbar(painter, rect, normalized_value(), wstate, hovered_left_,
                                    pressed_left_, hovered_right_, pressed_right_, hovered_thumb_);
}

bool Scrollbar::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Scroll) {
        if (hit_test(event.position)) {
            auto delta = event.scroll_dx != 0.0f ? event.scroll_dx : -event.scroll_dy;
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
            drag_start_mouse_ = event.position.x;
            drag_start_value_ = value_;
            return true;
        }
        // Click on track
        auto thumb = thumb_rect();
        if (event.position.x < thumb.x) {
            page_by(-step_page_);
        } else {
            page_by(step_page_);
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        pressed_left_ = false;
        pressed_right_ = false;
        dragging_ = false;
        stop_auto_repeat();
        return true;
    }

    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        hovered_left_ = hit_left_button(event.position);
        hovered_right_ = hit_right_button(event.position);
        hovered_thumb_ = hit_thumb(event.position);

        if (dragging_) {
            auto bs = button_size();
            auto track_w = rect_.width - 2 * bs;
            auto thumb_w = thumb_rect().width;
            auto max_thumb_x = track_w - thumb_w;
            if (max_thumb_x > 0) {
                auto mouse_delta = event.position.x - drag_start_mouse_;
                auto nv = (drag_start_value_ - min_) / (max_ - min_);
                nv += mouse_delta / max_thumb_x;
                set_normalized_value(nv);
            }
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Leave) {
        hovered_left_ = false;
        hovered_right_ = false;
        hovered_thumb_ = false;
        dragging_ = false;
        stop_auto_repeat();
        return true;
    }

    return false;
}

bool Scrollbar::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    switch (event.key) {
    case Key::Left:
        step_by(-step_small_);
        return true;
    case Key::Right:
        step_by(step_small_);
        return true;
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
        switch (event.key) {
        case Key::Up:
            step_by(-step_small_);
            return true;
        case Key::Down:
            step_by(step_small_);
            return true;
        default:
            break;
        }
    }

    return false;
}

} // namespace toolkit
