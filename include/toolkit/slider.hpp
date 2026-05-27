// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>

namespace toolkit {

enum class SliderOrientation { Horizontal, Vertical };

class Slider : public Widget, public Fluent<Slider> {
    DECLARE_WIDGET(Slider)
  public:
    explicit Slider(SliderOrientation orientation = SliderOrientation::Horizontal);
    nlohmann::json to_json() const override;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;

    float value() const { return value_; }
    Slider &set_value(float v);

    float minimum() const { return min_; }
    Slider &set_minimum(float m);

    float maximum() const { return max_; }
    Slider &set_maximum(float m);

    Slider &set_range(float min, float max);

    std::function<void(Slider &, float)> on_change;
    Slider &set_on_change(std::function<void(Slider &, float)> cb) {
        on_change = std::move(cb);
        return *this;
    }

  private:
    float value_to_pos(float v) const;
    float pos_to_value(float p) const;
    void update_value_from_pos(Point p);

    SliderOrientation orientation_;
    float value_ = 0.0f;
    float min_ = 0.0f;
    float max_ = 100.0f;
    bool dragging_ = false;
};

} // namespace toolkit
