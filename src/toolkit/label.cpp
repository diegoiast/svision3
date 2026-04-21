// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/label.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

namespace toolkit {

Label::Label() {}

Label::Label(std::string text) : text_(std::move(text)) {}

Label &Label::set_text(std::string const &text) {
    if (text_ == text) {
        return *this;
    }
    text_ = text;
    invalidate_layout();
    return *this;
}

void Label::paint(Painter &painter) {
    auto pallete = Theme::current().palette;
    auto fs = font_size_override_.value_or(pallete.fonts.size);
    auto col = color_override_.value_or(pallete.text);
    auto fm = painter.font_metrics(fs);
    auto display_text = text_;
    auto text_w = painter.measure_text(display_text, fs).width;

    if (elide_ && text_w > rect_.width && rect_.width > 0) {
        auto suffix = std::string_view{"..."};
        float sw = painter.measure_text(suffix, fs).width;
        if (sw < rect_.width) {
            while (!display_text.empty() && text_w + sw > rect_.width) {
                // Simplified: remove one byte at a time.
                // In a production app we should use Utf8Iterator to remove a full codepoint.
                display_text.pop_back();
                text_w = painter.measure_text(display_text, fs).width;
            }
            display_text += suffix;
            text_w = painter.measure_text(display_text, fs).width;
        }
    }

    auto text_x = 0.0f;
    auto baseline_y = (rect_.height - fm.height) / 2.0f + fm.ascent;

    if (alignment_ == Alignment::Center) {
        text_x = (rect_.width - text_w) / 2.0f;
    } else if (alignment_ == Alignment::End) {
        text_x = rect_.width - text_w;
    }
    painter.draw_text(display_text, {text_x, baseline_y}, col, fs);
}

bool Label::handle_mouse(MouseEvent const &) { return false; }

Size Label::size_hint() const {
    auto pallete = Theme::current().palette;
    auto font_size = font_size_override_.value_or(pallete.fonts.size);
    if (shrinkable_ || text_.empty()) {
        return {0, font_metrics(font_size).height + 4.0f};
    }
    return Theme::current().measure_label(text_, font_size);
}

} // namespace toolkit
