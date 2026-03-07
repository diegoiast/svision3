// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/events.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace toolkit {

class PlatformWindow;

struct Popup {
    Rect bounds;
    std::function<void(Painter &)> on_paint;
    std::function<bool(MouseEvent const &)> on_mouse;
    std::function<bool(KeyEvent const &)> on_key;
};

class Window {
  public:
    Window(std::string_view title, Size size);
    ~Window();

    Window(Window const &) = delete;
    Window &operator=(Window const &) = delete;

    void add_widget(std::unique_ptr<Widget> widget);
    void set_root(std::unique_ptr<Widget> root);
    void show();
    void close();
    void request_redraw();

    void open_popup(Popup popup);
    void close_popup();
    bool has_popup() const { return popup_.has_value(); }

    void handle_paint(Painter &painter);
    void handle_mouse(MouseEvent const &event);
    void handle_key(KeyEvent const &event);
    void handle_resize(Size new_size);

    void resize_to_fit();

    void set_cursor(CursorShape shape);

    void set_min_size(Size s);
    void set_max_size(Size s);
    Size min_size() const;
    Size max_size() const { return max_size_; }
    Size content_min_size() const;
    Size size() const { return size_; }

    int start_timer(float interval_sec, std::function<void()> callback, bool repeats = true);
    void stop_timer(int timer_id);

    void hide_tooltip();
    bool save_to_png(std::string const &path);
    float scale_factor() const;

    PlatformWindow *platform_window() const;

    void set_focused_widget(Widget *w);

  private:
    void focus_next(bool reverse);
    void update_tooltip(Widget *under, Point mouse_pos);
    void show_tooltip();
    void show_tooltip_window(std::string const &text, Point screen_pos);
    void hide_tooltip_window();

    void draw_debug_frames_recursive(Painter &painter, Widget *widget);

    std::string title_;
    Size size_;
    Size min_size_;
    Size max_size_;
    std::unique_ptr<Widget> root_;
    std::vector<std::unique_ptr<Widget>> widgets_;
    Widget *focused_widget_ = nullptr;
    std::optional<Popup> popup_;
    int blink_timer_id_ = 0;
    CursorShape current_cursor_ = CursorShape::Arrow;
    Widget *hovered_widget_ = nullptr;

    Widget *tooltip_widget_ = nullptr;
    int tooltip_timer_id_ = 0;
    bool tooltip_visible_ = false;
    Point tooltip_mouse_pos_;
    std::string tooltip_text_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace toolkit
