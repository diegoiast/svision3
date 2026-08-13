// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/theme.hpp"
#include "svision3/theme_base.hpp"
#include <memory>
#include <optional>

namespace svision3 {
class MaterialTheme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme, std::optional<Color> accent);

    explicit MaterialTheme(ColorScheme scheme = ColorScheme::Light,
                           std::optional<Palette> p = std::nullopt);
    Palette default_palette(ColorScheme scheme) const override;
};

class GnomeTheme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme, std::optional<Color> accent);

    explicit GnomeTheme(ColorScheme scheme = ColorScheme::Light,
                        std::optional<Palette> p = std::nullopt);
    Palette default_palette(ColorScheme scheme) const override;
    std::unique_ptr<Widget> create_title_bar(Window *window) const override;

    void draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                            WidgetState const &state) const override;
};

namespace ThemeFactory {
std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);
}

} // namespace svision3
