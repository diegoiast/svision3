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

static void parse_mnemonic(std::string const &raw, std::string &out_text, int &out_index,
                           char &out_key) {
    auto pos = raw.find('&');
    if (pos != std::string::npos && pos + 1 < raw.size()) {
        out_index = static_cast<int>(pos);
        out_key = static_cast<char>(std::tolower(static_cast<unsigned char>(raw[pos + 1])));
        out_text = raw.substr(0, pos) + raw.substr(pos + 1);
    } else {
        out_index = -1;
        out_key = 0;
        out_text = raw;
    }
}

Label::Label() {}

Label::Label(std::string text) {
    parse_mnemonic(text, text_, mnemonic_index_, mnemonic_key_);
}

Label &Label::set_text(std::string const &text) {
    std::string new_text;
    int new_index = -1;
    char new_key = 0;
    parse_mnemonic(text, new_text, new_index, new_key);
    if (text_ == new_text && mnemonic_index_ == new_index) {
        return *this;
    }
    text_ = std::move(new_text);
    mnemonic_index_ = new_index;
    mnemonic_key_ = new_key;
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

bool Label::trigger_mnemonic(char key) {
    if (!mnemonic_key_ || mnemonic_key_ != key || !buddy_) {
        return false;
    }
    if (buddy_->is_enabled() && buddy_->is_focusable()) {
        buddy_->set_focused(true);
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
