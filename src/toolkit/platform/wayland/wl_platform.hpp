#pragma once

#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/painters/cairo_shaper.hpp"
#include "toolkit/platform.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct wl_display;
struct wl_compositor;
struct wl_shm;
struct wl_seat;
struct wl_pointer;
struct wl_keyboard;
struct wl_surface;
struct wl_buffer;
struct wl_callback;
struct wl_cursor_theme;
struct wl_data_device_manager;
struct wl_data_device;
struct wl_data_offer;
struct wl_data_source;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_popup;
struct xdg_positioner;
struct wl_output;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct wl_egl_window;
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;
struct wp_viewporter;
struct wp_viewport;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;
struct xdg_wm_dialog_v1;
struct xdg_dialog_v1;
struct wp_cursor_shape_manager_v1;
struct wp_cursor_shape_device_v1;
struct zwp_pointer_constraints_v1;

namespace toolkit {

class WaylandPlatformWindow;

class WaylandPlatformApplication : public PlatformApplication {
    std::shared_ptr<ImageLoaderInterface> image_loader_;
    std::shared_ptr<SVGLoaderInterface> svg_loader_;

  public:
    WaylandPlatformApplication();
    ~WaylandPlatformApplication() override;
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
    std::string_view name() const override { return "Wayland"; }

    float scale_factor() const override;
    SystemFonts system_fonts() const override;
    std::string system_icon_theme() const override;
    bool needs_csd() const override { return decoration_manager == nullptr; }

    wl_display *display = nullptr;
    wl_compositor *compositor = nullptr;
    wl_shm *shm = nullptr;
    wl_seat *seat = nullptr;
    wl_pointer *pointer = nullptr;
    wl_keyboard *keyboard = nullptr;
    xdg_wm_base *wm_base = nullptr;
    wp_fractional_scale_manager_v1 *fractional_scale_manager = nullptr;
    wp_viewporter *viewporter = nullptr;
    zxdg_decoration_manager_v1 *decoration_manager = nullptr;
    xdg_wm_dialog_v1 *wm_dialog = nullptr;
    wp_cursor_shape_manager_v1 *cursor_shape_manager = nullptr;
    wp_cursor_shape_device_v1 *cursor_shape_device = nullptr;
    zwp_pointer_constraints_v1 *pointer_constraints = nullptr;
    wl_cursor_theme *cursor_theme = nullptr;
    wl_surface *cursor_surface = nullptr;
    wl_data_device_manager *data_device_manager = nullptr;
    wl_data_device *data_device = nullptr;
    xkb_context *xkb_ctx = nullptr;
    xkb_keymap *xkb_map = nullptr;
    xkb_state *xkb_st = nullptr;
    bool running = false;

    std::vector<WaylandPlatformWindow *> windows;
    WaylandPlatformWindow *pointer_focus = nullptr;
    WaylandPlatformWindow *keyboard_focus = nullptr;
    WaylandPlatformWindow *modal_window = nullptr;
    WaylandPlatformWindow *modal_parent = nullptr;
    float pointer_x = 0, pointer_y = 0;
    uint32_t pointer_enter_serial = 0;
    uint32_t pressed_button_serial = 0;

    bool mod_shift = false, mod_ctrl = false;
    bool mod_alt = false, mod_super = false;

    uint32_t last_click_time = 0, last_click_button = 0;
    float last_click_x = 0, last_click_y = 0;
    int click_count = 0;
    int pressed_button = -1;

    struct TimerEntry {
        int id;
        float interval_sec;
        bool repeats;
        std::function<void()> callback;
        std::chrono::steady_clock::time_point next_fire;
    };
    std::vector<TimerEntry> timers;
    int next_timer_id = 1;

    int wakeup_pipe[2] = {-1, -1};
    std::mutex posted_mutex;
    std::vector<std::function<void()>> posted_fns;
    std::string clipboard_content;
    // Cross-client clipboard state (wl_data_device_manager). `clipboard_source`
    // is non-null exactly while we own the selection (cleared by the compositor
    // "cancelled" event once another client takes it), which lets get_text()
    // shortcut to `clipboard_content` for self-paste instead of round-tripping
    // through our own data_source, same trick x11_platform.cpp uses with
    // clipboard_owner_window.
    wl_data_source *clipboard_source = nullptr;
    wl_data_offer *current_selection_offer = nullptr;
    wl_data_offer *dnd_offer = nullptr;
    std::map<wl_data_offer *, std::vector<std::string>> offer_mime_types;
    uint32_t input_serial = 0;
    std::vector<wl_output *> outputs;
    float output_scale = 1.0f;

    int32_t repeat_rate = 25;
    int32_t repeat_delay = 600;
    uint32_t repeating_key = 0;
    int repeat_timer_id = 0;

    void *egl_display = nullptr;
    void *egl_config = nullptr;
    void *egl_context = nullptr;
    bool opengl_requested = false;

    CairoShaper app_shaper_;
    std::unique_ptr<CairoTextRasterizer> app_rasterizer_;
};

class RenderingBackend;

class WaylandPlatformWindow : public PlatformWindow {
  public:
    WaylandPlatformWindow(WaylandPlatformApplication *app, std::string_view title, Size size,
                          Window *owner, WindowOptions options);
    ~WaylandPlatformWindow() override;
    void show() override;
    void close() override;
    void minimize() override;
    void maximize() override;
    void restore() override;
    void set_size(Size s) override;
    // A Wayland client is never told where its surface is and has no way to ask for a spot, so
    // position()/set_position() stay at their do-nothing defaults.
    bool can_set_position() const override { return false; }
    void request_redraw() override;
    void set_min_size(Size s) override;
    void set_max_size(Size s) override;
    int start_timer(float interval_sec, std::function<void()> callback, bool repeats) override;
    void stop_timer(int timer_id) override;
    void set_cursor(CursorShape shape) override;
    void set_icon(Icon const &) override {}
    Icon get_icon() override { return nullptr; }
    void show_system_menu(Point) override {}
    void start_system_move(uint32_t serial) override;
    void start_system_resize(WindowEdge edge, uint32_t serial) override;
    void show_tooltip_window(std::string const &text, Point pos) override;
    void hide_tooltip_window() override;
    void set_modal_for(PlatformWindow *parent) override;
    void grab_pointer() override {}
    void ungrab_pointer() override {}
    Icon capture() override;
    float scale_factor() const override;
    std::string_view painter_name() const override { return backend->name(); };
    void set_title(std::string_view t) override;

    void do_paint();
    void paint_tooltip();

    WaylandPlatformApplication *app_;
    Window *owner_;
    wl_surface *surface = nullptr;

    // FIXME: make a xdg_protocols struct with these variables
    xdg_surface *xdg_surface_ = nullptr;
    xdg_toplevel *xdg_toplevel_ = nullptr;
    xdg_popup *xdg_popup_ = nullptr;
    xdg_positioner *xdg_positioner_ = nullptr;
    xdg_dialog_v1 *xdg_dialog = nullptr;

    wl_callback *frame_cb = nullptr;
    wp_fractional_scale_v1 *fractional_scale = nullptr;
    wp_viewport *viewport = nullptr;
    zxdg_toplevel_decoration_v1 *toplevel_decoration = nullptr;
    float scale = 1.0f;
    bool configured = false;
    bool needs_redraw = true;
    // See x11_platform.cpp's identical flag for the full rationale: Window::maximize()/restore()
    // flip is_maximized_ synchronously at click time (so the button icon updates immediately),
    // but the compositor's xdg_toplevel_configure/xdg_surface_configure round-trip that actually
    // resizes the surface is async. A paint landing in between would use the new is_maximized_
    // (and thus new CSD shadow/corner-radius) with the still-old size -- the flicker this
    // suppresses.
    bool suppress_paint_for_maximize_transition = false;
    int pending_width = 0, pending_height = 0;
    CursorShape current_cursor = CursorShape::Arrow;

    wl_buffer *buffer = nullptr;
    void *shm_data = nullptr;
    int shm_fd = -1;
    size_t shm_size = 0;
    int buf_width = 0, buf_height = 0;
    wl_egl_window *egl_window = nullptr;
    void *egl_surface = nullptr;

    std::unique_ptr<RenderingBackend> backend;

    struct TooltipData {
        std::string text;
        wl_surface *surface = nullptr;
        xdg_surface *xdg_surf = nullptr;
        xdg_popup *popup = nullptr;
        wp_viewport *viewport = nullptr;
        wl_buffer *buffer = nullptr;
        void *shm_data = nullptr;
        int shm_fd = -1;
        size_t shm_size = 0;
        int width = 0, height = 0;
    };
    std::unique_ptr<TooltipData> tooltip_data;
};

} // namespace toolkit
