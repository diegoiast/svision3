// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"

namespace toolkit {

class ProgressBar : public Widget {
    DECLARE_WIDGET(ProgressBar)
  public:
    ProgressBar();
    nlohmann::json to_json() const override;
    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override {
        (void)event;
        return false;
    }
    Size size_hint() const override;

    void set_value(float v);
    float value() const { return value_; }

  private:
    float value_ = 0.0f;
};

} // namespace toolkit
