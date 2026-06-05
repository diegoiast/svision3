// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"

namespace toolkit {

class Button;
class HBoxLayout;
class Label;
class Window;

class WindowTitleBar : public Widget {
  public:
    WindowTitleBar(Window *window);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    void set_rect(Rect const &rect) override;
    Size size_hint() const override;

  private:
    auto create_btn(DecorationButton type) -> Button *;
    Window *window_;
    HBoxLayout *layout_;
    Label *title_label_;
    Button *max_btn_ = nullptr;
};

} // namespace toolkit
