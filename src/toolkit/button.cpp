// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/button.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cctype>

namespace toolkit {

void Button::on_state_changed() {
    if (window_) {
        window_->request_redraw("button state");
    }
    if (auto_repeat_ && state_handler_.button_state == ButtonState::ClickedInside) {
        start_auto_repeat_delay();
    }
}

bool Button::should_fire_click() const {
    return state_handler_.button_state == ButtonState::ClickedInside;
}

Button::Button(std::string text) {
    auto pos = text.find('&');
    auto const &style = Theme::current().button;

    state.focusable = true;
    if (pos != std::string::npos && pos + 1 < text.size()) {
        mnemonic_index_ = static_cast<int>(pos);
        mnemonic_key_ = static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
        display_text_ = text.substr(0, pos) + text.substr(pos + 1);
    } else {
        display_text_ = std::move(text);
    }

    auto_repeat_delay_ = style.auto_repeat_delay;
    auto_repeat_interval_ = style.auto_repeat_interval;

    state_handler_.on_state_change_callback = [this] { on_state_changed(); };
}

void Button::set_text(std::string text) {
    auto pos = text.find('&');
    std::string new_display_text;
    int new_mnemonic_index = -1;
    char new_mnemonic_key = 0;

    if (pos != std::string::npos && pos + 1 < text.size()) {
        new_mnemonic_index = static_cast<int>(pos);
        new_mnemonic_key =
            static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
        new_display_text = text.substr(0, pos) + text.substr(pos + 1);
    } else {
        new_display_text = std::move(text);
    }

    if (display_text_ == new_display_text && mnemonic_index_ == new_mnemonic_index) {
        return;
    }

    display_text_ = std::move(new_display_text);
    mnemonic_index_ = new_mnemonic_index;
    mnemonic_key_ = new_mnemonic_key;

    if (window_) {
        window_->request_redraw("button state");
    }
}

void Button::set_icon(Icon icon) {
    if (icon_ == icon) {
        return;
    }
    icon_ = std::move(icon);
    if (window_) {
        window_->request_redraw("button icon");
    }
}

void Button::clear_icon() {
    if (!icon_) {
        return;
    }
    icon_ = nullptr;
    if (window_) {
        window_->request_redraw("button icon");
    }
}

void Button::stop_auto_repeat() {
    if (auto_repeat_timer_id_ && window_) {
        window_->stop_timer(auto_repeat_timer_id_);
        auto_repeat_timer_id_ = 0;
    }
}

void Button::start_auto_repeat_delay() {
    stop_auto_repeat();
    if (window_ && auto_repeat_) {
        auto_repeat_timer_id_ = window_->start_timer(
            auto_repeat_delay_,
            [this] {
                if ((state_handler_.button_state == ButtonState::ClickedInside ||
                     state_handler_.button_state == ButtonState::ClickedOutside) &&
                    on_click) {
                    on_click();
                    start_auto_repeat_interval();
                } else {
                    auto_repeat_timer_id_ = 0;
                }
            },
            false);
    }
}

void Button::start_auto_repeat_interval() {
    stop_auto_repeat();
    if (window_ && auto_repeat_) {
        auto_repeat_timer_id_ = window_->start_timer(
            auto_repeat_interval_,
            [this] {
                if ((state_handler_.button_state == ButtonState::ClickedInside ||
                     state_handler_.button_state == ButtonState::ClickedOutside) &&
                    on_click) {
                    on_click();
                } else {
                    stop_auto_repeat();
                }
            },
            true);
    }
}

void Button::set_visible(bool v) {
    auto changed = state_handler_.button_state != ButtonState::Normal;

    if (is_visible() == v) {
        return;
    }
    Widget::set_visible(v);
    if (!is_visible()) {
        state_handler_.button_state = ButtonState::Normal;
        stop_auto_repeat();
        if (changed && window_) {
            window_->request_redraw("button state");
        }
    }
}

void Button::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto hovered = state_handler_.button_state == ButtonState::Hovered ||
                   state_handler_.button_state == ButtonState::ClickedInside;
    auto pressed = state_handler_.button_state == ButtonState::ClickedInside ||
                   state_handler_.button_state == ButtonState::ClickedOutside;
    Theme::current().draw_button(painter, rect, display_text_, icon_, hovered, pressed,
                                 is_focused(), is_enabled(), flat_, background_color_);
}

bool Button::trigger_mnemonic(char key) {
    if (!is_enabled()) {
        return false;
    }
    if (mnemonic_key_ && mnemonic_key_ == key) {
        if (on_click) {
            on_click();
        }
        return true;
    }
    return false;
}

bool Button::handle_key(KeyEvent const &event) {
    if (!is_enabled()) {
        return false;
    }
    if (Widget::handle_key(event)) {
        return true;
    }
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    if (event.key == Key::Enter || (!event.text.empty() && event.text[0] == ' ')) {
        if (on_click) {
            on_click();
        }
        return true;
    }
    return false;
}

bool Button::handle_mouse(MouseEvent const &event) {
    if (!is_enabled() || !is_visible()) {
        if (state_handler_.button_state != ButtonState::Normal) {
            state_handler_.button_state = ButtonState::Normal;
            stop_auto_repeat();
            if (window_) {
                window_->request_redraw("button state");
            }
        }
        return false;
    }
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
            if (auto_repeat_ && on_click && window_) {
                on_click();
                start_auto_repeat_delay();
            }
            return true;
        }
        return false;
    case MouseEvent::Type::Release:
        if (state_handler_.button_state == ButtonState::ClickedInside ||
            state_handler_.button_state == ButtonState::ClickedOutside) {
            auto fire_click = should_fire_click();
            stop_auto_repeat();
            state_handler_.on_mouse_click(event);
            if (fire_click && on_click && !auto_repeat_) {
                on_click();
            }
        }
        return inside;

    case MouseEvent::Type::Leave:
        state_handler_.on_mouse_leave();
        return true;
    case MouseEvent::Type::Drag:
        if (inside) {
            state_handler_.on_mouse_enter();
        } else {
            state_handler_.on_mouse_leave();
        }
        return inside;
    case MouseEvent::Type::Scroll:
        return false;
    }
    return false;
}

Size Button::size_hint() const { return Theme::current().measure_button(display_text_, icon_); }

} // namespace toolkit
