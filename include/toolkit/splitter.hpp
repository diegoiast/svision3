// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <memory>

namespace toolkit {

class Splitter : public Widget, public Fluent<Splitter> {
    DECLARE_WIDGET(Splitter)
  public:
    explicit Splitter(Orientation o = Orientation::Horizontal);

    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    Splitter &set_first(std::unique_ptr<Widget> w);
    Splitter &set_second(std::unique_ptr<Widget> w);
    Splitter &set_ratio(float r);
    float ratio() const { return ratio_; }
    Orientation orientation() const { return orientation_; }
    Splitter &set_locked(bool locked);
    bool locked() const { return locked_; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    CursorShape cursor() const override { return cursor_; }
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Size size_hint() const override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;
    void on_theme_changed() override;

  private:
    std::unique_ptr<Widget> first_;
    std::unique_ptr<Widget> second_;
    float ratio_ = 0.5f;
    Orientation orientation_;
    CursorShape cursor_ = CursorShape::Arrow;
    bool dragging_ = false;
    bool locked_ = false;
    int active_pane_ = -1; // 0 = first, 1 = second, -1 = none

    static constexpr float kHandleSize = 5.0f;
    static constexpr float kBorderWidth = 2.0f;

    float split_pos() const; // clamped pixel position of the split
    Rect handle_rect() const;
    void layout_children();
};

} // namespace toolkit
