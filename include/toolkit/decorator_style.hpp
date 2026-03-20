// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"
#include <functional>
#include <string>

namespace toolkit {

enum class ThemeStyle;
enum class ColorScheme;

struct DecoratorStyle {
    float title_height = 32.0f;
    float button_size = 16.0f;
    float button_spacing = 8.0f;
    float padding = 8.0f;
    Color title_text;
    Color title_bg;
    Color title_bg_inactive;
    Color button_close;
    Color button_minimize;
    Color button_maximize;
    Color button_hover;
    Color button_pressed;
    Color border;
    Color border_inactive;
};

struct DecoratorCallbacks {
    std::function<void()> on_close;
    std::function<void()> on_minimize;
    std::function<void()> on_maximize;
};

} // namespace toolkit
