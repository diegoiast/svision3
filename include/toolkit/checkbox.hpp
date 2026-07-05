// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button_state.hpp"
#include "toolkit/widget.hpp"
#include <functional>
#include <string>

namespace toolkit {

class Checkbox : public Widget {
    DECLARE_WIDGET(Checkbox)
  public:
    explicit Checkbox(std::string text);
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    bool trigger_mnemonic(char key) override;
    void collect_mnemonics(std::vector<Widget *> &out) override {
        if (mnemonic_key_) {
            out.push_back(this);
        }
    }

    bool checked() const { return state_ == CheckState::Checked; }
    void set_checked(bool c);

    CheckState check_state() const { return state_; }
    void set_check_state(CheckState newState);

    bool is_tri_state() const { return tri_state_; }
    void set_tri_state(bool t) { tri_state_ = t; }

    std::function<void(bool)> on_toggle;
    std::function<void(CheckState)> on_state_change;

  private:
    void toggle();
    void on_state_changed();
    bool should_fire_click() const;

    std::string text_;
    int mnemonic_index_ = -1;
    char mnemonic_key_ = 0;
    CheckState state_ = CheckState::Unchecked;
    bool tri_state_ = false;
    ButtonStateHandler state_handler_;
};

} // namespace toolkit
