// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/command.hpp"
#include "svision3/layout.hpp"
#include "svision3/widget.hpp"
#include <memory>
#include <vector>

namespace svision3 {

class Toolbar : public Widget, public Fluent<Toolbar> {
    DECLARE_WIDGET(Toolbar)
  public:
    Toolbar();

    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void add_command(Command::Ptr cmd);
    std::weak_ptr<Widget> add_widget(std::shared_ptr<Widget> w, float stretch = 0);
    void add_separator();

    void insert_command(int index, Command::Ptr cmd);
    void insert_separator(int index);
    void remove_range(int index, int count);
    int item_count() const;

    void clear();

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Widget *widget_at(Point p) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

  private:
    std::unique_ptr<HBoxLayout> layout_;
};

auto create_toolbar_separator() -> std::unique_ptr<Widget>;

} // namespace svision3
