// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/command.hpp"
#include "toolkit/events.hpp"
#include "toolkit/image.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/toast_widget.hpp"
#include "toolkit/types.hpp"
#include "toolkit/widget.hpp"

#include <algorithm>
#include <functional>
#include <memory>
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
    std::function<void()> on_close;
};

class Window {
  public:
    Window(std::string_view title, Size size, WindowOptions options = {});
    ~Window();

    Window(Window const &) = delete;
    Window &operator=(Window const &) = delete;

    void add_command(Command::Ptr cmd) { global_commands_.push_back(std::move(cmd)); }
    void remove_command(Command::Ptr const &cmd) {
        auto it = std::find(global_commands_.begin(), global_commands_.end(), cmd);
        if (it != global_commands_.end()) {
            global_commands_.erase(it);
        }
    }
    std::vector<Command::Ptr> const &global_commands() const { return global_commands_; }

    void add_widget(std::unique_ptr<Widget> widget);
    Window &set_root(std::unique_ptr<Widget> root);
    void on_theme_changed();
    void show();
    void close();
    void request_redraw(std::string_view reason = "other");

    void show_toast(std::string text, std::string title = "", std::string icon_path = "",
                    float timeout = 10.0f);
    void show_toast(ToastBuilder const &builder);
    void close_toast(ToastWidget *toast);
    void relayout_toasts();

    void open_popup(Popup popup);
    void close_popup();
    void close_all_popups();
    bool has_popup() const { return !popups_.empty(); }
    size_t num_popups() const { return popups_.size(); }

    std::function<bool(KeyEvent const &)> on_key;

    // Fired when the window moves to a display with a different DPI/scale factor.
    // The argument is the new scale (1.0 == 100%). Platforms deliver this from their
    // native per-window DPI-change notification, so it is event-driven, not polled.
    std::function<void(float)> on_scale_changed;

    void handle_paint(Painter &painter);
    void handle_mouse(MouseEvent const &event);
    void handle_key(KeyEvent const &event);
    void handle_resize(Size new_size);
    void handle_activate(bool active);
    void handle_maximized(bool maximized);

    // Called by platform backends when the native window's scale factor changes.
    // Deduplicates, relayouts, repaints, and fires on_scale_changed.
    void handle_scale_changed(float new_scale);
    bool is_active() const { return is_active_; }
    bool is_maximized() const { return is_maximized_; }
    void relayout();

    Window &resize_to_fit();

    void set_cursor(CursorShape shape);

    void set_options(WindowOptions options) { options_ = options; }
    WindowOptions options() const { return options_; }
    void set_csd_mode(bool csd) { options_.csd = csd; }

    // Combine the requested WindowOptions with a live query of the native window/WM/compositor,
    // so an external, out-of-band change to the native window (e.g. a raw Win32/Cocoa call)
    // can only ever make an action *more* restricted than what was requested, never less.
    bool is_resizable() const;
    bool is_movable() const;
    bool is_minimizable() const;
    bool is_maximizable() const;
    bool is_closable() const;

    void start_system_move(uint32_t serial);
    void start_system_resize(WindowEdge edge, uint32_t serial);
    void minimize();
    void maximize();
    void restore();

    std::string_view title() const { return title_; }
    void set_title(std::string_view t);
    void set_icon(Icon const &icon);
    Icon get_icon() const;
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
    nlohmann::json to_json() const;
    float scale_factor() const;

    PlatformWindow *platform_window() const;
    std::string_view painter_name() const;
    void grab_pointer();
    void ungrab_pointer();

    void set_focused_widget(Widget *w);
    Widget *focused_widget() const { return focused_widget_; }

    struct Statistics {
        uint64_t total_draws = 0;
        double avg_fps = 0;
        double avg_draw_time_ms = 0;
        double avg_repaint_time_ms = 0;
    };

    Statistics const &statistics() const;
    void reset_statistics();
    void set_statistics_logging_enabled(bool enabled);
    bool is_statistics_logging_enabled() const;

  private:
    bool dispatch_key_event_recursive(Widget *w, KeyEvent const &event);
    void focus_next(bool reverse);
    void update_tooltip(Widget *under, Point mouse_pos);
    void show_tooltip();
    void show_rich_tooltip();
    void show_tooltip_window(std::string const &text, Point screen_pos);
    void hide_tooltip_window();
    void start_toast_timer();

    void draw_debug_frames_recursive(Painter &painter, Widget *widget);
    void draw_widget_inspector(Painter &painter);

    std::string title_;
    Size size_;
    uint32_t last_serial_ = 0;
    WindowOptions options_;
    Size min_size_;
    Size max_size_;
    // FIXME: do we really need a "root" widget?
    std::unique_ptr<Widget> root_;
    std::vector<std::unique_ptr<Widget>> widgets_;
    std::vector<Command::Ptr> global_commands_;
    bool is_active_ = true;
    bool is_maximized_ = false;
    float scale_ = 1.0f;
    std::shared_ptr<bool> theme_observer_alive_ = std::make_shared<bool>(true);
    Widget *focused_widget_ = nullptr;
    Widget *saved_focus_ = nullptr;
    std::vector<Popup> popups_;
    CursorShape current_cursor_ = CursorShape::Arrow;
    Widget *hovered_widget_ = nullptr;
    Widget *captured_widget_ = nullptr;

    Widget *tooltip_widget_ = nullptr;
    int tooltip_timer_id_ = 0;
    bool tooltip_visible_ = false;
    Point tooltip_mouse_pos_;
    std::string tooltip_text_;

    int toast_timer_id_ = 0;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace toolkit
