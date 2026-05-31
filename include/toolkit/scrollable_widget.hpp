// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/scrollbar.hpp"
#include "toolkit/widget.hpp"
#include <memory>

namespace toolkit {

class ScrollableWidget : public Widget {
  public:
    ScrollableWidget();
    ~ScrollableWidget() override = default;

    void set_window(Window *w) override;
    void on_theme_changed() override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    void scroll_to(float x, float y);
    float scroll_x() const { return scroll_x_; }
    float scroll_y() const { return scroll_y_; }

  protected:
    void update_scrollbars(Size content_size);
    void draw_scrollbars(Painter &painter);
    bool handle_scrollbar_mouse(MouseEvent const &event);
    Rect viewport_rect() const;

    virtual void on_scroll(float /*x*/, float /*y*/) {}

    float scroll_x_ = 0;
    float scroll_y_ = 0;
    Size content_size_;

  private:
    std::unique_ptr<Scrollbar> vscroll_;
    std::unique_ptr<Scrollbar> hscroll_;

    // Overlay scrollbar geometry
    Rect vthumb_rect() const;
    Rect hthumb_rect() const;
    bool needs_vscroll() const;
    bool needs_hscroll() const;
    void clamp_scroll();
    void layout_scrollbars();

    static constexpr float kThumbWidth = 6.0f;
    static constexpr float kThumbMinLen = 20.0f;
    static constexpr float kScrollStep = 40.0f;
};

} // namespace toolkit
