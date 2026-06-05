// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme_base.hpp"
#include <memory>

namespace toolkit {

class Window;

class MacOSTheme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme, std::optional<Color> accent);

    explicit MacOSTheme(ColorScheme scheme = ColorScheme::Light,
                        std::optional<Palette> p = std::nullopt);

    Palette default_palette(ColorScheme scheme) const override;
    void draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                            WidgetState const &state) const override;
    void draw_tab_content_background(Painter &painter, Rect const &rect) const override;
    std::unique_ptr<Widget> create_title_bar(Window *window) const override;
};

} // namespace toolkit
