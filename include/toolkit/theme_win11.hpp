// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/theme_base.hpp"

namespace toolkit {

class Win11Theme : public BaseTheme {
  public:
    explicit Win11Theme(Palette p);

    void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title, bool hovered,
                           bool active, bool show_mnemonics, int mnemonic_index) const override;

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  bool hovered, bool enabled, TabOrientation orientation, bool has_close,
                  bool hovered_close) const override;

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override;
};

} // namespace toolkit
