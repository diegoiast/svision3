// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme_base.hpp"

namespace toolkit {

class MacOSTheme : public BaseTheme {
  public:
    explicit MacOSTheme(Palette p) : BaseTheme(std::move(p)) {
        name = "macOS";
        focus_ring_margin = 2.0f;
        focus_ring_corner_radius = 4.0f;
    }
};

} // namespace toolkit
