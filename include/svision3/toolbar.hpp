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

    Toolbar &set_orientation(Orientation o);
    Orientation orientation() const { return orientation_; }

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
    // The toolbar owns its items rather than the layout, because flipping
    // orientation swaps HBoxLayout for VBoxLayout -- two unrelated types with no
    // common item list to migrate.
    struct Entry {
        std::shared_ptr<Widget> widget;
        int stretch = 0;
    };

    void rebuild_layout();
    std::weak_ptr<Widget> insert_entry(int index, std::shared_ptr<Widget> w, int stretch);

    std::vector<Entry> entries_;
    std::unique_ptr<AbstractLayout> layout_;
    Orientation orientation_ = Orientation::Horizontal;
};

auto create_toolbar_separator(Orientation toolbar_orientation = Orientation::Horizontal)
    -> std::unique_ptr<Widget>;

} // namespace svision3
