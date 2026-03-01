// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <optional>
#include <string>

namespace toolkit {

class Label : public Widget {
  public:
    explicit Label(std::string text);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;

    void set_text(std::string const &text) { text_ = text; }
    std::string const &text() const { return text_; }

    void set_color(Color const &color) { color_override_ = color; }
    void set_font_size(float size) { font_size_override_ = size; }
    void set_shrinkable(bool s) { shrinkable_ = s; }
    bool shrinkable() const { return shrinkable_; }

  private:
    std::string text_;
    std::optional<Color> color_override_;
    std::optional<float> font_size_override_;
    bool shrinkable_ = false;
};

} // namespace toolkit
