// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <memory>
#include <string>

namespace toolkit {

class DockPanel : public Widget {
  public:
    explicit DockPanel(std::string title, std::unique_ptr<Widget> content = nullptr);

    void set_content(std::unique_ptr<Widget> content);
    Widget *content() const { return content_.get(); }
    std::string const &title() const { return title_; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    void for_each_child(std::function<void(Widget *)> const &cb) override;
    void collect_focusables(std::vector<Widget *> &out) override;

    static constexpr float title_bar_height = 24.0f;

  private:
    std::string title_;
    std::unique_ptr<Widget> content_;
};

class DockArea : public Widget {
  public:
    DockArea();

    void set_center(std::unique_ptr<Widget> w);
    void set_top(std::unique_ptr<Widget> w);
    void set_bottom(std::unique_ptr<Widget> w);
    void set_left(std::unique_ptr<Widget> w);
    void set_right(std::unique_ptr<Widget> w);

    Widget *center() const { return center_.get(); }
    Widget *top_widget() const { return top_.get(); }
    Widget *bottom_widget() const { return bottom_.get(); }
    Widget *left_widget() const { return left_.get(); }
    Widget *right_widget() const { return right_.get(); }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    void for_each_child(std::function<void(Widget *)> const &cb) override;
    void collect_focusables(std::vector<Widget *> &out) override;

  private:
    void apply_layout();
    std::unique_ptr<Widget> center_;
    std::unique_ptr<Widget> top_;
    std::unique_ptr<Widget> bottom_;
    std::unique_ptr<Widget> left_;
    std::unique_ptr<Widget> right_;
};

} // namespace toolkit
