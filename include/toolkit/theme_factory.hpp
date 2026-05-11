// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme.hpp"
#include "toolkit/theme_win11.hpp"
#include "toolkit/theme_plasma.hpp"
//#include "toolkit/theme_material.hpp"
#include <memory>
#include <optional>

namespace toolkit {
class MaterialTheme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme,
                                   std::optional<Color> accent);

    explicit MaterialTheme(ColorScheme scheme = ColorScheme::Light,
                           std::optional<Palette> p = std::nullopt);
    Palette default_palette(ColorScheme scheme) const override;
};

class GnomeTheme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme,
                                   std::optional<Color> accent);

    explicit GnomeTheme(ColorScheme scheme = ColorScheme::Light,
                        std::optional<Palette> p = std::nullopt);
    Palette default_palette(ColorScheme scheme) const override;
};

namespace ThemeFactory {
    std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);
}

} // namespace toolkit
