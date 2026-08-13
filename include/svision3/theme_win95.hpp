// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/theme_base.hpp"

namespace svision3 {

class Window;

class Win95Theme : public BaseTheme {
  public:
    static Palette default_palette(ColorScheme scheme, std::optional<Color> accent);

    explicit Win95Theme(ColorScheme scheme = ColorScheme::Light,
                        std::optional<Palette> p = std::nullopt);

    Palette default_palette(ColorScheme scheme) const override;

    std::unique_ptr<Widget> create_title_bar(Window *window) const override;

    void draw_focus_ring(Painter &painter, Rect const &rect, float corner_radius) const override;
    void draw_splitter_handle(Painter &painter, float pos, Rect const &splitter_rect,
                              Orientation orientation, bool hovered) const override;

    void draw_tree_item(Painter &painter, Rect const &rect, std::string_view text, int depth,
                        bool has_children, bool expanded, bool selected, bool hovered,
                        bool alternate) const override;

    void draw_progress_bar(Painter &painter, Rect const &rect, float progress,
                           WidgetState const &state) const override;

    void draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                            WidgetState const &state) const override;

    void draw_tab_content_background(Painter &painter, Rect const &rect) const override;
    void draw_list_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon,
                        bool selected, bool hovered, bool alternate) const override;

    void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active,
                  WidgetState const &state, TabOrientation orientation, bool has_close,
                  bool hovered_close, float font_size = 0.0f) const override;
};

} // namespace svision3
