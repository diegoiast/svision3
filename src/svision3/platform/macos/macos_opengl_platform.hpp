#pragma once

#include "macos_application_base.hpp"
#include "svision3/painters/gl_painter.hpp"

namespace svision3 {

class MacOSOpenGLPlatformApplication : public MacOSPlatformApplicationBase {
  public:
    std::unique_ptr<PlatformWindow> create_window(std::string_view title, Size size, Window *owner,
                                                  WindowOptions options) override;

    std::string_view name() const override { return "macOS"; }
    std::string_view painter_name() const override { return "OpenGL"; }
    PixelFormat native_pixel_format() const override { return PixelFormat::RGBA; }
};

class MacOSOpenGLPlatformWindow : public PlatformWindow {
  public:
    MacOSOpenGLPlatformWindow(std::string_view title, Size size, Window *owner,
                              WindowOptions options);
    ~MacOSOpenGLPlatformWindow() override;
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
    void set_icon(Icon const &icon) override;
    Icon get_icon() override;
    void show_system_menu(Point) override {}
    void start_system_move(uint32_t serial) override;
    void start_system_resize(WindowEdge edge, uint32_t serial) override;
    void show_tooltip_window(std::string const &text, Point pos) override;
    void hide_tooltip_window() override;
    Icon capture() override;
    float scale_factor() const override;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    Window *owner_;
};

} // namespace svision3
