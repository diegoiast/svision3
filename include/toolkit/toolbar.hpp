// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/command.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/widget.hpp"
#include <memory>
#include <vector>

namespace toolkit {

class Toolbar : public Widget {
  public:
    Toolbar();

    void add_command(Command::Ptr cmd);
    void add_widget(std::unique_ptr<Widget> w, float stretch = 0);
    void add_separator();

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Widget *widget_at(Point p) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

  private:
    std::unique_ptr<HBoxLayout> layout_;
};

} // namespace toolkit
