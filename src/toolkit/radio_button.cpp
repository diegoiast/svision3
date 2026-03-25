// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/radio_button.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>

namespace toolkit {

// --- RadioGroup ---

void RadioGroup::add(RadioButton *rb) { buttons_.push_back(rb); }

void RadioGroup::select(RadioButton *rb) {
    auto idx = 0;
    for (auto *b : buttons_) {
        b->set_selected(b == rb);
        if (b == rb && on_change) {
            on_change(idx);
        }
        idx++;
    }
}

RadioButton::RadioButton(std::string text, RadioGroup &group)
    : text_(std::move(text)), group_(group) {
    state.focusable = true;
    group_.add(this);
}

void RadioButton::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    Theme::current().draw_radio_button(painter, rect, text_, selected_, false, false, is_focused(),
                                       is_enabled());
}

bool RadioButton::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Press) {
        if (Rect{0, 0, rect_.width, rect_.height}.contains(event.position)) {
            group_.select(this);
            return true;
        }
    }
    return false;
}

bool RadioButton::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    if (event.key == Key::Enter || (!event.text.empty() && event.text[0] == ' ')) {
        group_.select(this);
        return true;
    }
    return false;
}

Size RadioButton::size_hint() const {
    auto const &style = Theme::current().radio;
    auto fm = Painter::measure_font_metrics(style.font_size);
    auto tw = Painter::measure_text(text_, style.font_size).width;
    auto h = std::max(style.box_size, fm.height);
    return {style.box_size + style.spacing + tw, h + 4.0f};
}

} // namespace toolkit
