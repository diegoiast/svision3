// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"
#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/painters/cairo_shaper.hpp"
#include "toolkit/platform.hpp"
#include <memory>

namespace toolkit {

class X11PlatformApplication : public PlatformApplication {
  public:
    X11PlatformApplication();
    ~X11PlatformApplication() override;
    std::unique_ptr<PlatformWindow> create_window(std::string_view title, Size size, Window *owner,
                                                  WindowOptions options) override;
    std::shared_ptr<ImageLoaderInterface> get_image_loader() override;
    std::shared_ptr<SVGLoaderInterface> get_svg_loader() override;
    PixelFormat native_pixel_format() const override;
    int run() override;
    void run_until(std::function<bool()> should_exit) override;
    void quit() override;
    void post_to_main_thread(std::function<void()> fn) override;
    std::string clipboard_get_text() override;
    void clipboard_set_text(std::string const &text) override;
    std::string_view name() const override { return "X11"; }
    float scale_factor() const override;
    SystemFonts system_fonts() const override;
    std::string system_icon_theme() const override;
    void add_fd_source(int fd, bool want_read, bool want_write, std::function<void()> callback) override;
    void remove_fd_source(int fd) override;

    // Forward to get X11 handles, without X11 includes
    void *get_display() const;
    void *get_visual() const;
    unsigned long get_net_wm_icon_atom() const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<ImageLoaderInterface> image_loader_;
    std::shared_ptr<SVGLoaderInterface> svg_loader_;
    CairoShaper app_shaper_;
    std::unique_ptr<CairoTextRasterizer> app_rasterizer_;
};

class X11PlatformWindow : public PlatformWindow {
  public:
    X11PlatformWindow(X11PlatformApplication *app, std::string_view title, Size size, Window *owner,
                      WindowOptions options);
    ~X11PlatformWindow() override;
    void show() override;
    void hide() override;
    void close() override;
    void minimize() override;
    void maximize() override;
    void restore() override;
    void set_size(Size s) override;
    bool can_set_position() const override { return true; }
    Point position() const override;
    void set_position(Point p) override;
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
    void grab_pointer() override;
    void ungrab_pointer() override;
    Icon capture() override;
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
