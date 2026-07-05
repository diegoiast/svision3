// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/radio_button.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <cctype>
#include <nlohmann/json.hpp>

namespace toolkit {

nlohmann::json RadioButton::to_json() const {
    auto j = Widget::to_json();
    j["text"] = text_;
    j["selected"] = selected_;
    return j;
}

void RadioButton::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        text_ = j["text"];
    }
    if (j.contains("selected")) {
        set_selected(j["selected"]);
    }
}

// --- RadioGroup ---

void RadioGroup::add(RadioButton *rb) { buttons_.push_back(rb); }

void RadioGroup::select(RadioButton *rb) {
    if (selected_ == rb) {
        return;
    }
    selected_ = rb;

    auto idx = 0;
    for (auto *b : buttons_) {
        b->set_selected(b == rb);
        if (b == rb && on_change) {
            on_change(idx);
        }
        idx++;
    }
}

void RadioButton::set_selected(bool s) {
    if (selected_ == s) {
        return;
    }
    selected_ = s;
    on_state_changed();
}

void RadioButton::on_state_changed() {
    if (window_) {
        window_->request_redraw("radio button state");
    }
}

bool RadioButton::should_fire_click() const {
    return state_handler_.button_state == ButtonState::ClickedInside;
}

RadioButton::RadioButton(std::string text, RadioGroup &group) : group_(group) {
    auto pos = text.find('&');
    if (pos != std::string::npos && pos + 1 < text.size()) {
        mnemonic_index_ = static_cast<int>(pos);
        mnemonic_key_ = static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
        text_ = text.substr(0, pos) + text.substr(pos + 1);
    } else {
        text_ = std::move(text);
    }
    state.focusable = true;
    group_.add(this);
    state_handler_.on_state_change_callback = [this] { on_state_changed(); };
}

void RadioButton::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto wstate = WidgetState{
        .interaction = state_handler_.button_state,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };
    Theme::current().draw_radio_button(painter, rect, text_, selected_, wstate);
}

bool RadioButton::handle_mouse(MouseEvent const &event) {
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

bool RadioButton::trigger_mnemonic(char key) {
    if (!is_enabled()) {
        return false;
    }
    if (mnemonic_key_ && mnemonic_key_ == key) {
        group_.select(this);
        return true;
    }
    return false;
}

} // namespace toolkit
