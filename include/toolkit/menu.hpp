// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/command.hpp"
#include "toolkit/events.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include <memory>
#include <vector>
#include <string>

namespace toolkit {

class Window;

struct MenuItem {
    enum class Type {
        Action,
        Separator,
        Submenu
    };

    Type type = Type::Action;
    std::shared_ptr<Command> command;
    std::shared_ptr<class Menu> submenu;

    bool is_separator() const { return type == Type::Separator; }
    bool is_action() const { return type == Type::Action; }
    bool is_submenu() const { return type == Type::Submenu; }

    static MenuItem action(std::shared_ptr<Command> cmd) {
        return {Type::Action, std::move(cmd), nullptr};
    }

    static MenuItem action(std::string name, std::function<void()> execute, bool enabled = true) {
        return action(std::make_shared<Command>(std::move(name), std::move(execute), enabled));
    }

    static MenuItem sep() {
        return {Type::Separator, nullptr, nullptr};
    }

    static MenuItem submenu_item(std::string name, std::shared_ptr<class Menu> menu);
};

class Menu : public std::enable_shared_from_this<Menu> {
  public:
    explicit Menu(std::string title = "");

    void add_action(std::shared_ptr<Command> cmd);
    void add_action(std::string name, std::function<void()> execute, bool enabled = true);
    void add_separator();
    void add_submenu(std::string name, std::shared_ptr<Menu> submenu);

    void show(Window *window, Point position);
    void close();

    std::string const &title() const { return title_; }
    std::string const &display_title() const { return display_title_; }
    char mnemonic_key() const { return mnemonic_key_; }
    int mnemonic_index() const { return mnemonic_index_; }
    std::vector<MenuItem> const &items() const { return items_; }
    void set_parent_menu(Menu *parent) { parent_menu_ = parent; }

    std::function<void()> on_request_next_menu;
    std::function<void()> on_request_prev_menu;
    std::function<void()> on_close_callback;

  private:
    friend class MenuBar;
    void paint(Painter &painter);
    bool handle_mouse(MouseEvent const &event);
    bool handle_key(KeyEvent const &event);
    int item_at(Point p) const;
    void open_submenu(int index);

    std::string title_;
    std::string display_title_;
    char mnemonic_key_ = 0;
    int mnemonic_index_ = -1;
    std::vector<MenuItem> items_;
    Window *window_ = nullptr;
    Rect bounds_;
    int hovered_ = -1;
    int open_submenu_index_ = -1;
    float item_height_ = 0;
    float separator_height_ = 7.0f;
    Menu *parent_menu_ = nullptr;
};

} // namespace toolkit
