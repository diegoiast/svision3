// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

namespace toolkit::chart_defaults {

// Shared plot-area margins used by chart widgets that render axes/labels around
// a central plotting rectangle. Left margin has two profiles depending on how
// wide the y-axis value labels are: charts formatting plain numbers ("123.45")
// use kMarginLeftNarrow, while volume/OHLC charts formatting wider strings
// ("1.2B") use kMarginLeftWide.
inline constexpr float kMarginLeftNarrow = 60;
inline constexpr float kMarginLeftWide = 70;
inline constexpr float kMarginRight = 16;
inline constexpr float kMarginTop = 30;
inline constexpr float kMarginBottom = 50;
inline constexpr float kLegendHeight = 20;

// Smallest data-axis span treated as non-degenerate; below this, a chart
// substitutes a fallback range so to_screen_x()/to_screen_y() never divide by
// (max - min) == 0.
inline constexpr float kMinDataRange = 1e-6f;

// Unit thresholds for compact large-number formatting (e.g. "1.2B", "45.0M").
inline constexpr float kBillion = 1e9f;
inline constexpr float kMillion = 1e6f;
inline constexpr float kThousand = 1e3f;

} // namespace toolkit::chart_defaults
