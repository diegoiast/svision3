// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <string>

namespace toolkit {

class Checkbox : public Widget {
  public:
    explicit Checkbox(std::string text);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;

    bool checked() const { return checked_; }
    void set_checked(bool c);

    std::function<void(bool)> on_toggle;

  private:
    void toggle();

    std::string text_;
    bool checked_ = false;
};

} // namespace toolkit
