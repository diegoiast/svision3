// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/events.hpp"
#include <functional>

namespace svision3 {

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

// Captures the full drawing state of a widget — passed to every Theme::draw_* method
// so themes have one consistent view of interaction, focus, enabled and window state.
struct WidgetState {
    ButtonState interaction = ButtonState::Normal;
    bool focused = false;
    bool enabled = true;
    bool window_active = true;
    bool checked = false;
};

} // namespace svision3
