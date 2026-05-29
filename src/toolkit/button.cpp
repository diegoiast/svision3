// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/button.hpp"
#include "toolkit/menu.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <cctype>
#include <nlohmann/json.hpp>

namespace toolkit {

nlohmann::json Button::to_json() const {
    auto j = Widget::to_json();
    j["text"] = display_text_;
    j["checked"] = checked_;
    j["checkable"] = checkable_;
    j["flat"] = flat_;
    return j;
}

void Button::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        set_text(j["text"]);
    }
    if (j.contains("checked")) {
        set_checked(j["checked"]);
    }
    if (j.contains("checkable")) {
        set_checkable(j["checkable"]);
    }
    if (j.contains("flat")) {
        set_flat(j["flat"]);
    }
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

    // FIXME: read values from system
    auto_repeat_delay_ = 1.0f;
    auto_repeat_interval_ = 0.5f;

    state_handler_.on_state_change_callback = [this] { on_state_changed(); };
}

void Button::on_state_changed() {
    if (window_) {
        window_->request_redraw("button state");
    }
    if (auto_repeat_ && state_handler_.button_state == ButtonState::ClickedInside) {
        start_auto_repeat_delay();
    }
}

Button &Button::set_menu(std::shared_ptr<Menu> menu) {
    menu_ = std::move(menu);
    invalidate_layout();
    return *this;
}

void Button::show_menu() {
    if (!menu_ || !window_) {
        return;
    }

    if (menu_->is_shown()) {
        menu_->close();
        menu_open_ = false;
        return;
    }

    menu_open_ = true;
    auto menu_pos = map_to_window({0, rect_.height});
    menu_->show(window_, menu_pos);
}

void Button::fire_click() {
    if (checkable_) {
        set_checked(!checked_);
    }
    if (command_) {
        command_->execute();
    }
    if (on_click) {
        on_click();
    }
}

void Button::sync_from_command() {
    if (!command_) {
        return;
    }
    set_text(command_->name());
    auto img = command_->icon_image();
    if (img) {
        set_icon(std::move(img));
    }
    Widget::set_enabled(command_->is_enabled());
    set_checked(command_->is_checked());
}

Button &Button::set_command(Command::Ptr cmd) {
    command_ = std::move(cmd);
    sync_from_command();
    return *this;
}

bool Button::should_fire_click() const {
    return state_handler_.button_state == ButtonState::ClickedInside;
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
                    (command_ || on_click)) {
                    fire_click();
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
                    (command_ || on_click)) {
                    fire_click();
                } else {
                    stop_auto_repeat();
                }
            },
            true);
    }
}

Widget &Button::set_visible(bool v) {
    auto changed = state_handler_.button_state != ButtonState::Normal;

    if (is_visible() == v) {
        return *this;
    }
    Widget::set_visible(v);
    if (!is_visible()) {
        state_handler_.button_state = ButtonState::Normal;
        stop_auto_repeat();
        if (changed && window_) {
            window_->request_redraw("button state");
        }
    }
    return *this;
}

void Button::paint(Painter &painter) {
    if (menu_open_ && window_ && !window_->has_popup()) {
        menu_open_ = false;
    }
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto interaction = state_handler_.button_state;
    if (menu_open_) {
        interaction = ButtonState::ClickedInside;
    }
    auto wstate = WidgetState{
        .interaction = interaction,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
        .checked = checked_,
    };
    Theme::current().draw_button(painter, rect, display_text_, icon_, wstate, flat_,
                                 background_color_);
    if (menu_) {
        Theme::current().draw_menu_indicator(painter, rect, is_enabled());
    }
}

bool Button::trigger_mnemonic(char key) {
    if (!is_enabled()) {
        return false;
    }
    if (mnemonic_key_ && mnemonic_key_ == key) {
        fire_click();
        return true;
    }
    return false;
}

Button &Button::set_flat(bool f) {
    flat_ = f;
    return *this;
}

Button &Button::set_checkable(bool c) {
    checkable_ = c;
    return *this;
}

Button &Button::set_checked(bool c) {
    if (checked_ != c) {
        checked_ = c;
        if (command_) {
            command_->set_checked(c);
        }
        if (on_toggle) {
            on_toggle(checked_);
        }
        if (window_) {
            window_->request_redraw("button checked changed");
        }
    }
    return *this;
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
        if (menu_) {
            show_menu();
        } else {
            fire_click();
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
            if (menu_) {
                show_menu();
                return true;
            }
            state_handler_.on_mouse_click(event);
            if (auto_repeat_ && (command_ || on_click) && window_) {
                fire_click();
                start_auto_repeat_delay();
            }
            return true;
        }
        return false;
    case MouseEvent::Type::Release:
        if (menu_) {
            if (inside && !menu_open_) {
                state_handler_.on_mouse_enter();
            } else {
                state_handler_.on_mouse_leave();
            }
            return inside;
        }
        if (state_handler_.button_state == ButtonState::ClickedInside ||
            state_handler_.button_state == ButtonState::ClickedOutside) {
            auto was_fire = should_fire_click();
            stop_auto_repeat();
            state_handler_.on_mouse_click(event);
            if (was_fire && !auto_repeat_) {
                fire_click();
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

Size Button::size_hint() const {
    auto sh = Theme::current().measure_button(display_text_, icon_);
    if (menu_) {
        sh.width += Theme::current().button.menu_indicator_width + 8.0f;
    }
    return sh;
}

} // namespace toolkit
