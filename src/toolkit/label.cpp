// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/label.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <cctype>
#include <nlohmann/json.hpp>

namespace toolkit {

nlohmann::json Label::to_json() const {
    auto j = Widget::to_json();
    j["text"] = text_;
    return j;
}

void Label::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        set_text(j["text"]);
    }
}

Label::Label() { set_frame(false); }

Label::Label(std::string text) {
    set_frame(false);
    mnemonic_key_ = parse_mnemonic(text).key;
    text_ = std::move(text);
}

Label &Label::set_text(std::string const &text) {
    if (text_ == text) {
        return *this;
    }
    mnemonic_key_ = parse_mnemonic(text).key;
    text_ = text;
    invalidate_layout();
    return *this;
}

void Label::paint(Painter &painter) {
    auto pallete = Theme::current().palette;
    auto fs = font_size_override_.value_or(pallete.fonts.size);
    auto col = color_override_.value_or(pallete.text);
    auto fm = painter.font_metrics(fs);

    auto display_text = strip_mnemonic(text_);

    auto text_w = painter.measure_text(display_text, fs).width;
    std::string_view draw_text = text_;

    if (elide_ && text_w > rect_.width && rect_.width > 0) {
        auto suffix = std::string_view{"..."};
        float sw = painter.measure_text(suffix, fs).width;
        if (sw < rect_.width) {
            while (!display_text.empty() && text_w + sw > rect_.width) {
                display_text.pop_back();
                text_w = painter.measure_text(display_text, fs).width;
            }
            display_text += suffix;
            text_w = painter.measure_text(display_text, fs).width;
        }
        draw_text = display_text;
    }

    auto text_x = 0.0f;
    auto baseline_y = (rect_.height - fm.height) / 2.0f + fm.ascent;

    if (alignment_ == Alignment::Center) {
        text_x = (rect_.width - text_w) / 2.0f;
    } else if (alignment_ == Alignment::End) {
        text_x = rect_.width - text_w;
    }

    painter.draw_mnemonic_text(draw_text, {text_x, baseline_y}, col, fs);
}

bool Label::handle_mouse(MouseEvent const &) { return false; }

bool Label::trigger_mnemonic(std::string_view key) {
    if (mnemonic_key_.empty() || mnemonic_key_ != key || !buddy_) {
        return false;
    }
    if (buddy_->is_enabled() && buddy_->is_focusable() && buddy_->window()) {
        buddy_->window()->set_focused_widget(buddy_);
        return true;
    }
    return false;
}

Size Label::size_hint() const {
    auto pallete = Theme::current().palette;
    auto font_size = font_size_override_.value_or(pallete.fonts.size);
    if (shrinkable_ || text_.empty()) {
        return {0, font_metrics(font_size).height + 4.0f};
    }
    return Theme::current().measure_label(text_, font_size);
}

} // namespace toolkit
