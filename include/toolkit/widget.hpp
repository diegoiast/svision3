// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/events.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include <functional>
#include <functional>
#include <optional>
#include <string>
#include <vector>


namespace toolkit {

class Window;

class Widget {
  public:
    virtual ~Widget() = default;

    static bool debug_show_frames;

    void draw(Painter &painter);
    virtual void paint(Painter &painter) = 0;
    virtual bool handle_mouse(MouseEvent const &event) = 0;
    virtual bool handle_key(KeyEvent const &event) {
        (void)event;
        return false;
    }
    virtual Size size_hint() const { return {0, 0}; }

    virtual void set_rect(Rect const &rect) {
        if (rect_ == rect) {
            return;
        }
        rect_ = rect;
        layout_dirty = true;
    }
    Rect const &rect() const { return rect_; }
    bool hit_test(Point p) const {
        return p.x >= 0 && p.x <= rect_.width && p.y >= 0 && p.y <= rect_.height;
    }

    void set_layout_dirty(bool dirty) { layout_dirty = dirty; }
    bool is_layout_dirty() const { return layout_dirty; }
    void invalidate_layout();

    virtual bool focusable() const { return focusable_; }
    void set_focusable(bool f) { focusable_ = f; }
    virtual void set_focused(bool focused) { focused_ = focused; }
    bool is_focused() const { return focused_; }

    void set_enabled(bool e) { enabled_ = e; }
    bool is_enabled() const { return enabled_; }

    virtual void set_visible(bool v) { visible_ = v; }
    bool is_visible() const { return visible_; }
    void show() { visible_ = true; }
    void hide() { visible_ = false; }

    void set_min_size(Size s) { min_size_ = s; }
    void set_max_size(Size s) { max_size_ = s; }
    Size min_size() const { return min_size_; }
    Size max_size() const { return max_size_; }

    virtual Widget *find_focusable_at(Point p) {
        if (focusable() && enabled_ && visible_ && hit_test(p)) {
            return this;
        }
        return nullptr;
    }

    virtual void collect_focusables(std::vector<Widget *> &out) {
        if (focusable() && enabled_ && visible_) {
            out.push_back(this);
        }
    }

    virtual CursorShape cursor() const { return CursorShape::Arrow; }

    virtual Widget *widget_at(Point p) {
        if (visible_ && hit_test(p)) {
            return this;
        }
        return nullptr;
    }

    virtual bool trigger_mnemonic(char /*key*/) { return false; }
    virtual void collect_mnemonics(std::vector<Widget *> &out) { (void)out; }

    virtual void for_each_child(std::function<void(Widget *)> const &callback) { (void)callback; }

    static bool dispatch_mouse_event(Widget *w, MouseEvent const &event);

    virtual void set_window(Window *w) { window_ = w; }
    Window *window() const { return window_; }

    void set_parent(Widget *p) { parent_ = p; }
    Widget *parent() const { return parent_; }

    void set_tooltip(std::string text) { tooltip_ = std::move(text); }
    std::string const &tooltip() const { return tooltip_; }

    void set_background_color(std::optional<Color> c) { background_color_ = c; }
    std::optional<Color> background_color() const { return background_color_; }

  protected:
    Rect rect_;
    bool layout_dirty = true;
    bool focused_ = false;
    bool enabled_ = true;
    bool visible_ = true;
    bool focusable_ = false;
    Size min_size_;
    Size max_size_;
    Window *window_ = nullptr;
    Widget *parent_ = nullptr;
    std::string tooltip_;
    std::optional<Color> background_color_;
};

} // namespace toolkit
