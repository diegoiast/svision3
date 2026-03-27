// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/radio_button.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
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

void RadioButton::on_state_changed() {
    if (window_) {
        window_->request_redraw("radio button state");
    }
}

bool RadioButton::should_fire_click() const {
    return state_handler_.button_state == ButtonState::ClickedInside;
}

RadioButton::RadioButton(std::string text, RadioGroup &group)
    : text_(std::move(text)), group_(group) {
    state.focusable = true;
    group_.add(this);
    state_handler_.on_state_change_callback = [this] { on_state_changed(); };
}

void RadioButton::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto pressed = state_handler_.button_state == ButtonState::ClickedInside;
    Theme::current().draw_radio_button(painter, rect, text_, selected_, pressed, false,
                                       is_focused(), is_enabled());
}

bool RadioButton::handle_mouse(MouseEvent const &event) {
    auto inside = Rect{0, 0, rect_.width, rect_.height}.contains(event.position);

    switch (event.type) {
    case MouseEvent::Type::Move:
        if (inside) {
            state_handler_.on_mouse_enter();
        } else {
            state_handler_.on_mouse_leave();
        }
        return inside;
    case MouseEvent::Type::Press:
        if (inside) {
            state_handler_.on_mouse_click(event);
            return true;
        }
        return false;
    case MouseEvent::Type::Release:
        if (state_handler_.button_state == ButtonState::ClickedInside && inside) {
            state_handler_.on_mouse_click(event);
            group_.select(this);
        } else if (state_handler_.button_state == ButtonState::ClickedOutside) {
            state_handler_.on_mouse_click(event);
        } else if (state_handler_.button_state == ButtonState::ClickedInside && !inside) {
            state_handler_.on_mouse_click(event);
        }
        return inside;
    case MouseEvent::Type::Leave:
        state_handler_.on_mouse_leave();
        return true;
    default:
        break;
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

Size RadioButton::size_hint() const { return Theme::current().measure_radio_button(text_); }

} // namespace toolkit
