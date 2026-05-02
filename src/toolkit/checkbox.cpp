// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/checkbox.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

namespace toolkit {

void Checkbox::on_state_changed() {
    if (window_) {
        window_->request_redraw("checkbox state");
    }
}

bool Checkbox::should_fire_click() const {
    return state_handler_.button_state == ButtonState::ClickedInside;
}

Checkbox::Checkbox(std::string text) : text_(std::move(text)) {
    state.focusable = true;
    state_handler_.on_state_change_callback = [this] { on_state_changed(); };
}

void Checkbox::set_checked(bool c) {
    set_check_state(c ? CheckState::Checked : CheckState::Unchecked);
}

void Checkbox::set_check_state(CheckState newState) {
    if (state_ == newState) {
        return;
    }
    state_ = newState;
    if (window_) {
        window_->request_redraw("checkbox state");
    }
}

void Checkbox::toggle() {
    if (tri_state_) {
        if (state_ == CheckState::Unchecked) {
            state_ = CheckState::Checked;
        } else if (state_ == CheckState::Checked) {
            state_ = CheckState::Partial;
        } else {
            state_ = CheckState::Unchecked;
        }
    } else {
        state_ = (state_ == CheckState::Checked) ? CheckState::Unchecked : CheckState::Checked;
    }

    if (on_toggle) {
        on_toggle(state_ == CheckState::Checked);
    }
    if (on_state_change) {
        on_state_change(state_);
    }
    if (window_) {
        window_->request_redraw("checkbox state");
    }
}

void Checkbox::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto wstate = WidgetState{
        .interaction   = state_handler_.button_state,
        .focused       = is_focused(),
        .enabled       = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };
    Theme::current().draw_checkbox(painter, rect, text_, state_, wstate);
}

bool Checkbox::handle_mouse(MouseEvent const &event) {
    auto inside = Rect{0, 0, rect_.width, rect_.height}.contains(event.position);

    switch (event.type) {
    case MouseEvent::Type::Move:
    case MouseEvent::Type::Drag:
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
            toggle();
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

bool Checkbox::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    if (event.key == Key::Enter || (!event.text.empty() && event.text[0] == ' ')) {
        toggle();
        return true;
    }
    return false;
}

Size Checkbox::size_hint() const { return Theme::current().measure_checkbox(text_); }

} // namespace toolkit
