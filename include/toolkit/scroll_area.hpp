// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <memory>

namespace toolkit {

// A generic scrollable container.  Set any widget as content with set_content();
// the widget receives its natural size (from size_hint()) and ScrollArea clips
// and scrolls it within the available viewport.
//
// Scrollbars are drawn as thin overlay thumbs (no space taken from the viewport).
// Horizontal scrolling is supported but the scrollbar is only shown when needed.
class ScrollArea : public Widget {
  public:
    ScrollArea();
    ~ScrollArea() override = default;

    void set_content(std::unique_ptr<Widget> widget);
    Widget *content() const { return content_.get(); }

    // Force a scroll position (clamped to valid range).
    void scroll_to(float x, float y);
    float scroll_x() const { return scroll_x_; }
    float scroll_y() const { return scroll_y_; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    CursorShape cursor() const override {
        return content_ ? content_->cursor() : CursorShape::Arrow;
    }
    Size size_hint() const override { return {0, 0}; }
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    void on_theme_changed() override;

    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

  private:
    static constexpr float kThumbWidth = 6.0f;
    static constexpr float kThumbMinLen = 20.0f;
    static constexpr float kScrollStep = 40.0f;

    std::unique_ptr<Widget> content_;
    float scroll_x_ = 0;
    float scroll_y_ = 0;

    // Scrollbar drag state
    bool dragging_v_ = false;
    bool dragging_h_ = false;
    float drag_start_mouse_ = 0;
    float drag_start_scroll_ = 0;

    float content_w() const;
    float content_h() const;
    bool needs_vscroll() const;
    bool needs_hscroll() const;

    // Thumb geometry in widget-local coords.
    Rect vthumb_rect() const;
    Rect hthumb_rect() const;

    void clamp_scroll();
    void update_content_rect();
};

} // namespace toolkit
