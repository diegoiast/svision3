// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button_state.hpp"
#include "toolkit/widget.hpp"
#include <functional>
#include <string>
#include <vector>

namespace toolkit {

class RadioButton;

class RadioGroup {
  public:
    void add(RadioButton *rb);
    void select(RadioButton *rb);

    std::function<void(int index)> on_change;

  private:
    // FIXME: naked pointers are bad,
    std::vector<RadioButton *> buttons_;
};

class RadioButton : public Widget {
  public:
    RadioButton(std::string text, RadioGroup &group);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;

    bool selected() const { return selected_; }
    void set_selected(bool s) { selected_ = s; }

  private:
    friend class RadioGroup;
    void on_state_changed();
    bool should_fire_click() const;

    std::string text_;
    RadioGroup &group_;
    bool selected_ = false;
    ButtonStateHandler state_handler_;
};

} // namespace toolkit
