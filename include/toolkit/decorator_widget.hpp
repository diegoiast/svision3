// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/decorator_style.hpp"
#include "toolkit/widget.hpp"
#include <memory>

namespace toolkit {

class DecoratorWidget : public Widget {
  public:
    DecoratorWidget(DecoratorStyle style, std::string title);
    void set_title(std::string title);
    void set_callbacks(DecoratorCallbacks callbacks);
    Rect client_area() const;
    Point content_offset() const;
    void paint_background(Painter &painter);
    void paint_borders(Painter &painter);

  protected:
    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;

  private:
    DecoratorStyle style_;
    std::string title_;
    DecoratorCallbacks callbacks_;
    float border_width_ = 1.0f;
    enum class Button { None, Close, Minimize, Maximize };
    Button hovered_button_ = Button::None;
    Button pressed_button_ = Button::None;

    Rect close_button() const;
    Rect minimize_button() const;
    Rect maximize_button() const;
};

} // namespace toolkit
