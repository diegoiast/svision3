// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <chrono>
#include <functional>
#include <string>

namespace toolkit {

class SpinBox : public Widget, public Fluent<SpinBox> {
  public:
    explicit SpinBox(int value = 0, int min_val = 0, int max_val = 100, int step = 1);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override;
    void set_focused(bool focused) override;

    int value() const { return value_; }
    SpinBox &set_value(int v);
    SpinBox &set_range(int min_val, int max_val);
    SpinBox &set_step(int step) {
        step_ = step;
        return *this;
    }

    std::function<void(int value)> on_change;

  private:
    enum class HitZone { None, Field, Up, Down };
    HitZone hit_zone(Point pos) const;
    Rect up_btn_rect() const;
    Rect down_btn_rect() const;
    float btn_width() const;
    void step_up();
    void step_down();
    void commit_text();
    void sync_text();

    int value_;
    int min_val_;
    int max_val_;
    int step_;
    std::string text_;
    bool editing_ = false;
    size_t cursor_pos_ = 0;
    size_t sel_anchor_ = 0;
    HitZone hovered_zone_ = HitZone::None;
    HitZone pressed_zone_ = HitZone::None;
    std::chrono::steady_clock::time_point cursor_blink_time_;
};

} // namespace toolkit
