// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/command.hpp"
#include "toolkit/events.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include <memory>
#include <vector>

namespace toolkit {

class Window;

struct MenuItem {
    std::shared_ptr<Command> command;
    bool separator = false;

    static MenuItem action(std::string name, std::function<void()> execute,
                           bool enabled = true) {
        return {std::make_shared<Command>(std::move(name), std::move(execute), enabled)};
    }

    static MenuItem sep() { return {nullptr, true}; }
};

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

    std::vector<MenuItem> items_;
    Window *window_ = nullptr;
    Rect bounds_;
    int hovered_ = -1;
    float item_height_ = 0;
    float separator_height_ = 7.0f;
};

} // namespace toolkit
