// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/button_state.hpp"
#include "svision3/command.hpp"
#include "svision3/events.hpp"
#include "svision3/painter.hpp"
#include "svision3/types.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace svision3 {

class Window;

struct MenuItem {
    enum class Type { Action, Separator, Submenu };

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

    static MenuItem sep() { return {Type::Separator, nullptr, nullptr}; }

    static MenuItem submenu_item(std::string name, std::shared_ptr<class Menu> menu);
};

class Menu : public std::enable_shared_from_this<Menu> {
  public:
    // Why a menu went away. The menubar needs to tell these apart: Escape hands
    // control back to the menubar with the title still highlighted, while running
    // a command or clicking elsewhere leaves menu navigation altogether.
    enum class CloseReason { Dismissed, ActionTriggered, EscapePressed };

    explicit Menu(std::string title = "");

    void add_action(std::shared_ptr<Command> cmd);
    void add_action(std::string name, std::function<void()> execute, bool enabled = true);
    void add_separator();
    void add_submenu(std::string name, std::shared_ptr<Menu> submenu);

    // Positional editing, for menus whose contents depend on what the application
    // is doing -- contextual commands that belong to the focused widget, recent
    // file lists, and so on. Sizing happens in show(), so edit a closed menu.
    void insert_action(int index, std::shared_ptr<Command> cmd);
    void insert_separator(int index);
    void remove_range(int index, int count);
    int item_count() const { return static_cast<int>(items_.size()); }
    void clear();

    void show(Window *window, Point position);
    void close();
    bool is_shown() const { return window_ != nullptr; }
    void select_first();

    std::string const &title() const { return title_; }
    std::string const &display_title() const { return display_title_; }
    std::string const &mnemonic_key() const { return mnemonic_key_; }
    int hovered_item() const { return hovered_; }
    Rect bounds() const { return bounds_; }
    std::vector<MenuItem> const &items() const { return items_; }
    void set_parent_menu(Menu *parent) { parent_menu_ = parent; }
    CloseReason last_close_reason() const { return close_reason_; }

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
    std::string mnemonic_key_;
    std::vector<MenuItem> items_;
    Window *window_ = nullptr;
    Rect bounds_;
    int hovered_ = -1;
    int pressed_item_ = -1;
    int open_submenu_index_ = -1;
    ButtonStateHandler state_handler_;
    float item_height_ = 0;
    float separator_height_ = 7.0f;
    Menu *parent_menu_ = nullptr;
    CloseReason close_reason_ = CloseReason::Dismissed;
};

} // namespace svision3
