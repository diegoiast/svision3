// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/events.hpp"
#include <functional>

namespace toolkit {

enum class ButtonState {
    Normal,
    Hovered,
    ClickedInside,
    ClickedOutside,
};

class ButtonStateHandler {
  public:
    ButtonState button_state = ButtonState::Normal;
    std::function<void()> on_state_change_callback;

    virtual ~ButtonStateHandler() = default;

    virtual bool should_fire_click() const { return false; }

    void on_mouse_enter();
    void on_mouse_leave();
    bool on_mouse_click(const MouseEvent &event);
    bool on_keyboard(const KeyEvent &event);
};

} // namespace toolkit
