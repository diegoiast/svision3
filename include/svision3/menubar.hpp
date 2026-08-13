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

    static auto item_to_json(MenuItem const &item) -> nlohmann::json;
    static void item_from_json(nlohmann::json const &j, Menu &menu);

    std::vector<std::shared_ptr<Menu>> menus_;
    int hovered_ = -1;
    int active_ = -1;
    bool alt_key_down_ = false;
    bool menu_bar_keyboard_active_ = false;
    bool show_mnemonics_ = false;
};

} // namespace svision3
