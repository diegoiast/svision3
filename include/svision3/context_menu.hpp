// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/button_state.hpp"
#include "svision3/menu.hpp"
#include <memory>
#include <vector>

namespace svision3 {

class Window;

class ContextMenu {
  public:
    explicit ContextMenu(std::vector<MenuItem> items);

    void show(Window *window, Point position);

  private:
    void paint(Painter &painter);
    bool handle_mouse(MouseEvent const &event);
    bool handle_key(KeyEvent const &event);
    void close();
    int item_at(Point p) const;
    void open_submenu(int index);

    std::vector<MenuItem> items_;
    Window *window_ = nullptr;
    Rect bounds_;
    int hovered_ = -1;
    int pressed_item_ = -1;
    int open_submenu_index_ = -1;
    float item_height_ = 0;
    float separator_height_ = 7.0f;
    ButtonStateHandler state_handler_;
};

} // namespace svision3
