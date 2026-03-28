// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <optional>
#include <string>

namespace toolkit {

class Label : public Widget, public Fluent<Label> {
  public:
    explicit Label();
    explicit Label(std::string text);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;

    Label &set_text(std::string const &text);
    std::string const &text() const { return text_; }

    Label &set_color(Color const &color) {
        color_override_ = color;
        return *this;
    }
    Label &set_font_size(float size) {
        font_size_override_ = size;
        return *this;
    }
    Label &set_shrinkable(bool s) {
        shrinkable_ = s;
        return *this;
    }
    bool shrinkable() const { return shrinkable_; }

    Label &set_alignment(Alignment a) {
        alignment_ = a;
        return *this;
    }
    Alignment alignment() const { return alignment_; }

    Label &set_elide(bool e) {
        elide_ = e;
        return *this;
    }
    bool elide() const { return elide_; }

  private:
    std::string text_;
    std::optional<Color> color_override_;
    std::optional<float> font_size_override_;
    bool shrinkable_ = false;
    Alignment alignment_ = Alignment::Start;
    bool elide_ = true;
};

} // namespace toolkit
