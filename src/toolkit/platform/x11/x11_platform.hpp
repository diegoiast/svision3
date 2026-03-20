// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/platform.hpp"
#include <memory>

namespace toolkit {

class X11PlatformApplication : public PlatformApplication {
  public:
    X11PlatformApplication();
    ~X11PlatformApplication() override;
    std::unique_ptr<PlatformWindow> create_window(std::string_view title, Size size,
                                                  Window *owner) override;
    int run() override;
    void quit() override;
    void post_to_main_thread(std::function<void()> fn) override;
    std::string clipboard_get_text() override;
    void clipboard_set_text(std::string const &text) override;
    Size measure_text(std::string_view text, float font_size,
                      FontFamily font = FontFamily::System) override;
    Painter::FontMetrics measure_font_metrics(float font_size,
                                              FontFamily font = FontFamily::System) override;
    std::string_view name() const override { return "X11"; }
    std::string_view painter_name() const override;
    float scale_factor() const override;
    SystemFonts system_fonts() const override;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class X11PlatformWindow : public PlatformWindow {
  public:
    X11PlatformWindow(X11PlatformApplication *app, std::string_view title, Size size,
                      Window *owner);
    ~X11PlatformWindow() override;
    void show() override;
    void close() override;
    void minimize() override;
    void maximize() override;
    void set_size(Size s) override;
    void request_redraw() override;
    void set_min_size(Size s) override;
    void set_max_size(Size s) override;
    int start_timer(float interval_sec, std::function<void()> callback, bool repeats) override;
    void stop_timer(int timer_id) override;
    void set_cursor(CursorShape shape) override;
    void show_tooltip_window(std::string const &text, Point pos) override;
    void hide_tooltip_window() override;
    bool save_to_png(std::string const &path) override;
    float scale_factor() const override;

    void do_paint();

    struct Impl;
    std::unique_ptr<Impl> impl_;

  private:
    void cleanup_resources();
    X11PlatformApplication *app_;
    Window *owner_;
};

} // namespace toolkit
