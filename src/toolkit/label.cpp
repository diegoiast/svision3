// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/label.hpp"
#include "toolkit/theme.hpp"

namespace toolkit {

Label::Label(std::string text) : text_(std::move(text)) {}

void Label::set_text(std::string const &text) {
    if (text_ == text) return;
    text_ = text;
    invalidate_layout();
}

void Label::paint(Painter &painter) {
    auto const &style = Theme::current().label;
    auto fs = font_size_override_.value_or(style.font_size);
    auto col = color_override_.value_or(style.text);
    auto fm = painter.font_metrics(fs);

    std::string display_text = text_;
    float text_w = painter.text_size(display_text, fs).width;

    if (elide_ && text_w > rect_.width && rect_.width > 0) {
        std::string suffix = "...";
        float sw = painter.text_size(suffix, fs).width;
        if (sw < rect_.width) {
            while (!display_text.empty() && text_w + sw > rect_.width) {
                // Simplified: remove one byte at a time.
                // In a production app we should use Utf8Iterator to remove a full codepoint.
                display_text.pop_back();
                text_w = painter.text_size(display_text, fs).width;
            }
            display_text += suffix;
            text_w = painter.text_size(display_text, fs).width;
        }
    }

    float text_x = rect_.x;
    if (alignment_ == Alignment::Center) {
        text_x = rect_.x + (rect_.width - text_w) / 2.0f;
    } else if (alignment_ == Alignment::End) {
        text_x = rect_.x + rect_.width - text_w;
    }

    auto baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;
    painter.draw_text(display_text, {text_x, baseline_y}, col, fs);
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
