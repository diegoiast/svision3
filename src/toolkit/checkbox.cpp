// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/checkbox.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <cctype>
#include <nlohmann/json.hpp>

namespace toolkit {

nlohmann::json Checkbox::to_json() const {
    auto j = Widget::to_json();
    j["text"] = text_;
    j["state"] = static_cast<int>(state_);
    j["tri_state"] = tri_state_;
    return j;
}

void Checkbox::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        text_ = j["text"];
    }
    if (j.contains("state")) {
        set_check_state(static_cast<CheckState>(j["state"].get<int>()));
    }
    if (j.contains("tri_state")) {
        set_tri_state(j["tri_state"]);
    }
}

void Checkbox::on_state_changed() {
    if (window_) {
        window_->request_redraw("checkbox state");
    }
}

bool Checkbox::should_fire_click() const {
    return state_handler_.button_state == ButtonState::ClickedInside;
}

Checkbox::Checkbox(std::string text) {
    auto pos = text.find('&');
    if (pos != std::string::npos && pos + 1 < text.size()) {
        mnemonic_key_ = static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
    }
    text_ = std::move(text);
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
        .interaction = state_handler_.button_state,
        .focused = is_focused(),
        .enabled = is_enabled(),
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

bool Checkbox::trigger_mnemonic(char key) {
    if (!is_enabled()) {
        return false;
    }
    if (mnemonic_key_ && mnemonic_key_ == key) {
        toggle();
        return true;
    }
    return false;
}

} // namespace toolkit
