// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/menu.hpp"
#include "toolkit/widget.hpp"
#include <memory>
#include <vector>

namespace toolkit {

class MenuBar : public Widget {
  public:
    MenuBar();

    void add_menu(std::shared_ptr<Menu> menu);
    std::shared_ptr<Menu> add_menu(std::string title);

    void open_menu(int index);
    int find_menu(std::string_view title) const;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    bool trigger_mnemonic(char key) override;
    void set_show_mnemonics(bool show);

  private:
    int menu_at(Point p) const;

    std::vector<std::shared_ptr<Menu>> menus_;
    int hovered_ = -1;
    int active_ = -1;
    bool alt_key_down_ = false;
    bool menu_bar_keyboard_active_ = false;
    bool show_mnemonics_ = false;
};

} // namespace toolkit
