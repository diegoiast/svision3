// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/button_state.hpp"

namespace svision3 {

void ButtonStateHandler::on_mouse_enter() {
    switch (button_state) {
    case ButtonState::ClickedOutside:
        button_state = ButtonState::ClickedInside;
        if (on_state_change_callback) {
            on_state_change_callback();
        }
        break;
    case ButtonState::Hovered:
        break;
    case ButtonState::Normal:
        button_state = ButtonState::Hovered;
        if (on_state_change_callback) {
            on_state_change_callback();
        }
        break;
    case ButtonState::ClickedInside:
        break;
    }
}

void ButtonStateHandler::on_mouse_leave() {
    switch (button_state) {
    case ButtonState::ClickedInside:
        button_state = ButtonState::ClickedOutside;
        if (on_state_change_callback) {
            on_state_change_callback();
        }
        break;
    case ButtonState::ClickedOutside:
        break;
    case ButtonState::Hovered:
        button_state = ButtonState::Normal;
        if (on_state_change_callback) {
            on_state_change_callback();
        }
        break;
    case ButtonState::Normal:
        break;
    }
}

bool ButtonStateHandler::on_mouse_click(const MouseEvent &event) {
    auto handled = false;
    auto is_press = (event.type == MouseEvent::Type::Press);

    switch (button_state) {
    case ButtonState::ClickedInside:
        handled = true;
        if (!is_press) {
            button_state = ButtonState::Hovered;
            if (on_state_change_callback) {
                on_state_change_callback();
            }
        }
        break;
    case ButtonState::ClickedOutside:
        if (is_press) {
            button_state = ButtonState::ClickedInside;
            handled = true;
            if (on_state_change_callback) {
                on_state_change_callback();
            }
        } else {
            button_state = ButtonState::Normal;
            if (on_state_change_callback) {
                on_state_change_callback();
            }
        }
        break;
    case ButtonState::Hovered:
        if (is_press) {
            handled = true;
            button_state = ButtonState::ClickedInside;
            if (on_state_change_callback) {
                on_state_change_callback();
            }
        }
        break;
    case ButtonState::Normal:
        if (is_press) {
            handled = true;
            button_state = ButtonState::ClickedInside;
            if (on_state_change_callback) {
                on_state_change_callback();
            }
        }
        break;
    }
    return handled;
}

bool ButtonStateHandler::on_keyboard(const KeyEvent &event) {
    if (event.type == KeyEvent::Type::Press) {
        if (event.key == Key::Enter || event.key == Key::Tab) {
            return true;
        }
    }
    return false;
}

} // namespace svision3
