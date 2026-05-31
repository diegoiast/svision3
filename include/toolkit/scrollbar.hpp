// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button_state.hpp"
#include "toolkit/widget.hpp"
#include <functional>

namespace toolkit {

class Scrollbar : public Widget, public Fluent<Scrollbar> {
    DECLARE_WIDGET(Scrollbar)
  public:
    explicit Scrollbar(Orientation o = Orientation::Horizontal);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;

    Scrollbar &set_value(float v);
    float value() const { return value_; }

    Scrollbar &set_range(float min, float max);
    float min() const { return min_; }
    float max() const { return max_; }

    Scrollbar &set_step_small(float s) {
        step_small_ = s;
        return *this;
    }
    Scrollbar &set_step_page(float s) {
        step_page_ = s;
        return *this;
    }

    std::function<void(float)> on_change;

  private:
    bool hit_left_button(Point p) const;
    bool hit_right_button(Point p) const;
    bool hit_thumb(Point p) const;
    float button_size() const;
    Rect track_rect() const;
    Rect thumb_rect() const;
    Rect left_button_rect() const;
    Rect right_button_rect() const;

    float normalized_value() const;
    void set_normalized_value(float v);
    void step_by(float delta);
    void page_by(float delta);
    void go_to_min();
    void go_to_max();

    static constexpr float kButtonSize = 16.0f;
    static constexpr float kMinThumbSize = 20.0f;

    float value_ = 0.0f;
    float min_ = 0.0f;
    float max_ = 100.0f;
    float step_small_ = 1.0f;
    float step_page_ = 10.0f;

    bool hovered_left_ = false;
    bool pressed_left_ = false;
    bool hovered_right_ = false;
    bool pressed_right_ = false;
    bool hovered_thumb_ = false;
    bool dragging_ = false;
    float drag_start_mouse_ = 0;
    float drag_start_value_ = 0;
    int auto_repeat_timer_id_ = 0;

    void stop_auto_repeat();
    void start_auto_repeat(float direction);

    Orientation orientation_;
};

} // namespace toolkit
