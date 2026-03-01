#pragma once

#include "macos_application_base.hpp"
#include "toolkit/painters/gl_painter.hpp"

namespace toolkit {

class MacOSOpenGLPlatformApplication : public MacOSPlatformApplicationBase {
  public:
    std::unique_ptr<PlatformWindow> create_window(std::string_view title,
                                                   Size size,
                                                   Window *owner) override;
    std::string_view name() const override { return "macOS"; }
    std::string_view painter_name() const override { return "OpenGL"; }
};

class MacOSOpenGLPlatformWindow : public PlatformWindow {
  public:
    MacOSOpenGLPlatformWindow(std::string_view title, Size size, Window *owner);
    ~MacOSOpenGLPlatformWindow() override;
    void show() override;
    void close() override;
    void request_redraw() override;
    void set_min_size(Size s) override;
    void set_max_size(Size s) override;
    int start_timer(float interval_sec, std::function<void()> callback,
                    bool repeats) override;
    void stop_timer(int timer_id) override;
    void set_cursor(CursorShape shape) override;
    void show_tooltip_window(std::string const &text, Point pos) override;
    void hide_tooltip_window() override;
    bool save_to_png(std::string const &path) override;
    float scale_factor() const override;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    Window *owner_;
};

} // namespace toolkit
