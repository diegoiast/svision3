// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme_base.hpp"

namespace toolkit {

class Window;

class Win95Theme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme, std::optional<Color> accent);

    explicit Win95Theme(ColorScheme scheme = ColorScheme::Light,
                        std::optional<Palette> p = std::nullopt);

    Palette default_palette(ColorScheme scheme) const override;

    std::unique_ptr<Widget> create_title_bar(Window *window) const override;

    void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const override;

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override;

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           WidgetState const &state) const override;
};

} // namespace toolkit
