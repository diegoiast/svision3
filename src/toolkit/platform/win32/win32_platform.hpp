#pragma once

#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/platform.hpp"
#include <memory>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace toolkit {

class Win32PlatformApplication : public PlatformApplication {
  public:
    Win32PlatformApplication();
    ~Win32PlatformApplication() override;
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
    std::string_view name() const override { return "Win32"; }
    float scale_factor() const override;
    SystemFonts system_fonts() const override;

    HINSTANCE hinstance = nullptr;
    DWORD main_thread_id = 0;
    ULONG_PTR gdiplus_token = 0;
    bool opengl_requested = false;

    struct WindowData {
        Window *owner = nullptr;
        HCURSOR current_cursor = nullptr;
        DWORD last_press_time = 0;
        int last_press_x = 0, last_press_y = 0;
        int last_press_button = -1;
        int click_count = 0;
    };
    std::unordered_map<HWND, WindowData> window_map;

    struct TimerInfo {
        int toolkit_id;
        Window *window;
        HWND hwnd;
        bool repeats;
        std::function<void()> callback;
    };
    std::unordered_map<UINT_PTR, TimerInfo> timers;
    UINT_PTR next_timer_id = 1;

    std::mutex posted_mutex;
    std::vector<std::function<void()>> posted_fns;

    static constexpr wchar_t kWindowClassName[] = L"TKWindow";
    static constexpr wchar_t kTooltipClassName[] = L"TKTooltip";
    static constexpr UINT WM_TK_INVOKE = WM_APP + 1;
};

class Win32PlatformWindow : public PlatformWindow {
  public:
    Win32PlatformWindow(Win32PlatformApplication *app, std::string_view title, Size size,
                        Window *owner);
    ~Win32PlatformWindow() override;
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
    std::string_view painter_name() const override;

    Win32PlatformApplication *app_;
    Window *owner_;
    HWND hwnd = nullptr;
    HWND tooltip_hwnd = nullptr;
    HGLRC hglrc = nullptr;
    HCURSOR arrow_cursor = nullptr, ibeam_cursor = nullptr;
    HCURSOR hand_cursor = nullptr, not_allowed_cursor = nullptr;
    Win32TextRasterizer rasterizer_;
    std::unique_ptr<RenderingBackend> backend_;
};

LRESULT CALLBACK tk_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK tk_tooltip_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
Win32PlatformApplication *win32_app_instance();

} // namespace toolkit
