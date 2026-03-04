// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

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

  private:
    friend class RadioGroup;
    void set_selected(bool s) { selected_ = s; }

    std::string text_;
    RadioGroup &group_;
    bool selected_ = false;
};

} // namespace toolkit
