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

    bool checked() const { return state_ == CheckState::Checked; }
    void set_checked(bool c);

    CheckState check_state() const { return state_; }
    void set_check_state(CheckState s);

    bool is_tri_state() const { return tri_state_; }
    void set_tri_state(bool t) { tri_state_ = t; }

    std::function<void(bool)> on_toggle;
    std::function<void(CheckState)> on_state_change;

  private:
    void toggle();

    std::string text_;
    CheckState state_ = CheckState::Unchecked;
    bool tri_state_ = false;
};

} // namespace toolkit
