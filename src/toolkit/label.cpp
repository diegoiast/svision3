// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/label.hpp"
#include "toolkit/theme.hpp"

namespace toolkit {

Label::Label(std::string text) : text_(std::move(text)) {}

void Label::paint(Painter &painter) {
    auto const &style = Theme::current().label;
    auto fs = font_size_override_.value_or(style.font_size);
    auto col = color_override_.value_or(style.text);
    auto fm = painter.font_metrics(fs);
    // FIXME: what is this extra 2.0f gap?
    auto baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;

    painter.draw_text(text_, {rect_.x, baseline_y}, col, fs);
}

bool Label::handle_mouse(MouseEvent const &) { return false; }

Size Label::size_hint() const {
    auto const &style = Theme::current().label;
    auto font_size = font_size_override_.value_or(style.font_size);
    auto font_metrics = Painter::measure_font_metrics(font_size);
    auto w = (!shrinkable_ && !text_.empty()) ? Painter::measure_text(text_, font_size).width : 0;

    // FIXME: what is this extra 4.0f gap?
    return {w, font_metrics.height + 4.0f};
}

} // namespace toolkit
