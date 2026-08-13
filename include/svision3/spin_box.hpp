// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/button.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace svision3 {

class SpinBox : public Widget, public Fluent<SpinBox> {
    DECLARE_WIDGET(SpinBox)
  public:
    explicit SpinBox(int value = 0, int min_val = 0, int max_val = 100, int step = 1);
    nlohmann::json to_json() const override;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override;
    SpinBox &set_focused(bool focused) override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Widget *widget_at(Point p) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    int value() const { return value_; }
    SpinBox &set_value(int v);
    SpinBox &set_range(int min_val, int max_val);
    SpinBox &set_step(int step) {
        step_ = step;
        return *this;
    }

    std::function<void(int value, SpinBox &)> on_change;

  private:
    void step_up();
    void step_down();
    void commit_text();
    void sync_text();
    void relayout_buttons();

    int value_;
    int min_val_;
    int max_val_;
    int step_;
    std::string text_;
    bool editing_ = false;
    size_t cursor_pos_ = 0;
    size_t sel_anchor_ = 0;
    std::chrono::steady_clock::time_point cursor_blink_time_;

    std::unique_ptr<Button> up_button_;
    std::unique_ptr<Button> down_button_;
};

} // namespace svision3
