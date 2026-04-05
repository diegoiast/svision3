// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme_base.hpp"

namespace toolkit {

class Win95Theme : public BaseTheme {
  public:
    explicit Win95Theme(Palette p);

    void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const override;

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override;

    void draw_tree_background(Painter &painter, Rect const &rect, bool focused) const override;

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           bool enabled) const override;
};

} // namespace toolkit
