// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme_base.hpp"

namespace toolkit {

class MacOSTheme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme,
                                   std::optional<Color> accent);

    explicit MacOSTheme(ColorScheme scheme = ColorScheme::Light,
                        std::optional<Palette> p = std::nullopt);

    Palette default_palette(ColorScheme scheme) const override;
};

} // namespace toolkit
