#pragma once

#include "toolkit/platform.hpp"
#include <memory>

namespace toolkit {

class DummyPlatformWindow : public PlatformWindow {
  public:
    void set_client_side_decorations(bool) override {}
    bool client_side_decorations() const override { return false; }
    void show() override {}
    void close() override {}
    void minimize() override {}
    void maximize() override {}
    void set_size(Size) override {}
    void request_redraw() override {}
    void set_min_size(Size) override {}
    void set_max_size(Size) override {}
    int start_timer(float, std::function<void()>, bool) override { return 0; }
    void stop_timer(int) override {}
    void set_cursor(CursorShape) override {}
    void show_tooltip_window(std::string const &, Point) override {}
    void hide_tooltip_window() override {}
    bool save_to_png(std::string const &) override { return true; }
    float scale_factor() const override { return 1.0f; }
};

class DummyPlatformApplication : public PlatformApplication {
  public:
    bool client_side_decorations() const override { return false; }
    std::unique_ptr<PlatformWindow> create_window(std::string_view title, Size size, Window *owner,
                                                  bool csd) override;
    int run() override { return 0; }
    void quit() override {}
    void post_to_main_thread(std::function<void()> fn) override { fn(); }
    std::string clipboard_get_text() override { return ""; }
    void clipboard_set_text(std::string const &text) override {}
    Size measure_text(std::string_view text, float font_size,
                      FontFamily font = FontFamily::System) override;
    Painter::FontMetrics measure_font_metrics(float font_size,
                                              FontFamily font = FontFamily::System) override;
    std::string_view name() const override { return "dummy"; }
    std::string_view painter_name() const override { return "none"; }
    float scale_factor() const override { return 1.0f; }
    SystemFonts system_fonts() const override { return {"sans", "mono", 14.0f}; }
};

struct DummyPlatformGuard {
    DummyPlatformApplication dummy;
    DummyPlatformGuard() { detail::set_current_platform(&dummy); }
    ~DummyPlatformGuard() { detail::set_current_platform(nullptr); }
};

} // namespace toolkit
