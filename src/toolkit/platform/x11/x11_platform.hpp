// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"
#include "toolkit/platform.hpp"
#include <memory>

namespace toolkit {

class X11PlatformApplication : public PlatformApplication {
  public:
    X11PlatformApplication();
    ~X11PlatformApplication() override;
    std::unique_ptr<PlatformWindow> create_window(std::string_view title, Size size, Window *owner,
                                                  WindowOptions options) override;
    std::unique_ptr<ImageLoaderInterface> create_image_loader() override;
    int run() override;
    void run_until(std::function<bool()> should_exit) override;
    void quit() override;
    void post_to_main_thread(std::function<void()> fn) override;
    std::string clipboard_get_text() override;
    void clipboard_set_text(std::string const &text) override;
    std::string_view name() const override { return "X11"; }
    float scale_factor() const override;
    SystemFonts system_fonts() const override;

    // Forward to get X11 handles, without X11 includes
    void *get_display() const;
    void *get_visual() const;
    unsigned long get_net_wm_icon_atom() const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class X11PlatformWindow : public PlatformWindow {
  public:
    X11PlatformWindow(X11PlatformApplication *app, std::string_view title, Size size, Window *owner,
                      WindowOptions options);
    ~X11PlatformWindow() override;
    void show() override;
    void close() override;
    void minimize() override;
    void maximize() override;
    void restore() override;
    void set_size(Size s) override;
    void request_redraw() override;
    void set_min_size(Size s) override;
    void set_max_size(Size s) override;
    int start_timer(float interval_sec, std::function<void()> callback, bool repeats) override;
    void stop_timer(int timer_id) override;
    void set_cursor(CursorShape shape) override;
    void set_title(std::string_view t) override;
    void set_icon(Icon const &icon) override;
    Icon get_icon() override;
    void show_system_menu(Point p) override;
    void start_system_move(uint32_t serial) override;
    void start_system_resize(WindowEdge edge, uint32_t serial) override;
    void show_tooltip_window(std::string const &text, Point pos) override;
    void hide_tooltip_window() override;
    void set_modal_for(PlatformWindow *parent) override;
    bool save_to_png(std::string const &path) override;
    float scale_factor() const override;
    std::string_view painter_name() const override;

    void do_paint();

    struct Impl;
    std::unique_ptr<Impl> impl_;

  private:
    void cleanup_resources();
    X11PlatformApplication *app_;
    Window *owner_;
    Icon icon_;
};

} // namespace toolkit
