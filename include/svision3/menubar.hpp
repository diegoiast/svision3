// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/menu.hpp"
#include "svision3/widget.hpp"
#include <memory>
#include <vector>

namespace svision3 {

class MenuBar : public Widget, public Fluent<MenuBar> {
    DECLARE_WIDGET(MenuBar)
  public:
    MenuBar();

    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void add_menu(std::shared_ptr<Menu> menu);
    std::shared_ptr<Menu> add_menu(std::string title);

    void toggle_menu(int index);
    int find_menu(std::string_view title) const;

    // The shortcut registry: every enabled command reachable from this menubar,
    // at any submenu depth, is matched against the event. Returns nullptr when
    // nothing claims it.
    Command::Ptr find_shortcut(KeyEvent const &event) const;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    bool trigger_mnemonic(std::string_view key) override;
    void set_show_mnemonics(bool show);

  private:
    float get_menu_x(int index) const;
    int menu_at(Point p) const;
    bool mnemonics_visible() const;
    // An empty menu is not drawn and cannot be reached: it has nothing to show, so
    // its title would be a dead word in the menubar.
    bool is_menu_shown(int index) const;
    // Enters or leaves keyboard navigation, taking and releasing the window's
    // keyboard grab with it.
    void set_keyboard_active(bool active);
    // Shared by F10 and a solitary Alt press: enter keyboard menu navigation, or
    // leave it if already there.
    void toggle_keyboard_activation();
    bool has_open_menu() const;

    static auto item_to_json(MenuItem const &item) -> nlohmann::json;
    static void item_from_json(nlohmann::json const &j, Menu &menu);

    std::vector<std::shared_ptr<Menu>> menus_;
    int hovered_ = -1;
    int active_ = -1;
    bool alt_key_down_ = false;
    // Alt is only an accelerator on its own; any other key pressed while it is
    // held turns the chord into a mnemonic and cancels the activation.
    bool alt_pressed_alone_ = false;
    bool menu_bar_keyboard_active_ = false;
    bool show_mnemonics_ = false;
};

} // namespace svision3
