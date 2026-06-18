#pragma once

#include "toolkit/image.hpp"
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
    std::shared_ptr<ImageLoaderInterface> image_loader_;
    std::shared_ptr<SVGLoaderInterface> svg_loader_;

  public:
    Win32PlatformApplication();
    ~Win32PlatformApplication() override;
    std::unique_ptr<PlatformWindow> create_window(std::string_view title, Size size, Window *owner,
                                                  WindowOptions options) override;
    std::shared_ptr<ImageLoaderInterface> get_image_loader() override;
    std::shared_ptr<SVGLoaderInterface> get_svg_loader() override;
    int run() override;
    void run_until(std::function<bool()> should_exit) override;
    void quit() override;
    void post_to_main_thread(std::function<void()> fn) override;
    std::string clipboard_get_text() override;
    void clipboard_set_text(std::string const &text) override;
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

    static void paint_window(HWND hwnd, Window *win);

  private:
    Win32TextRasterizer app_rasterizer_;
};

class Win32PlatformWindow : public PlatformWindow {
  public:
    Win32PlatformWindow(Win32PlatformApplication *app, std::string_view title, Size size,
                        Window *owner, WindowOptions options);
    ~Win32PlatformWindow() override;
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
    void grab_pointer() override;
    void ungrab_pointer() override;
    Icon capture() override;
    float scale_factor() const override;
    std::string_view painter_name() const override;

    Win32PlatformApplication *app_;
    Window *owner_;
    HWND hwnd = nullptr;
    HWND modal_parent_hwnd = nullptr;
    HWND tooltip_hwnd = nullptr;
    HICON hicon = nullptr;
    Icon icon_;
    HGLRC hglrc = nullptr;
    HCURSOR arrow_cursor = nullptr, ibeam_cursor = nullptr;
    HCURSOR hand_cursor = nullptr, not_allowed_cursor = nullptr;
    HCURSOR resize_ew_cursor = nullptr, resize_ns_cursor = nullptr;
    HCURSOR resize_nw_cursor = nullptr, resize_nesw_cursor = nullptr, move_cursor = nullptr;
    Win32TextRasterizer rasterizer_;
    std::unique_ptr<RenderingBackend> backend_;
};

LRESULT CALLBACK tk_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK tk_tooltip_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
Win32PlatformApplication *win32_app_instance();

} // namespace toolkit
