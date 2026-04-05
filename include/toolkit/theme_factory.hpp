// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme.hpp"
#include <memory>

namespace toolkit {

class ThemeFactory {
  public:
    static std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);
    static std::unique_ptr<Theme> create(ThemeStyle style, Palette const &palette);
};

} // namespace toolkit
