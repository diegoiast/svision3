// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <memory>
#include <vector>

namespace toolkit {

class VBoxLayout : public Widget {
  public:
    void add_widget(std::unique_ptr<Widget> widget, int stretch = 0,
                    Alignment h_align = Alignment::Fill);

    void set_margins(Margins const &m) { margins_ = m; }
    void set_spacing(float s) { spacing_ = s; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;

  private:
    void apply_layout();

    struct Item {
        std::unique_ptr<Widget> widget;
        int stretch;
        Alignment h_align;
    };
    std::vector<Item> items_;
    Margins margins_;
    float spacing_ = 8.0f;
};

class HBoxLayout : public Widget {
  public:
    void add_widget(std::unique_ptr<Widget> widget, int stretch = 0,
                    Alignment v_align = Alignment::Center);

    void set_margins(Margins const &m) { margins_ = m; }
    void set_spacing(float s) { spacing_ = s; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;

  private:
    void apply_layout();

    struct Item {
        std::unique_ptr<Widget> widget;
        int stretch;
        Alignment v_align;
    };
    std::vector<Item> items_;
    Margins margins_;
    float spacing_ = 8.0f;
};

} // namespace toolkit
