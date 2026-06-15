#include "wl_platform.hpp"
#include "../linux_utils.hpp"
#include "toolkit/application.hpp"
#include "toolkit/stb_image_loader.hpp"
#include "toolkit/lunasvg_image_loader.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <cstring>

#include "cursor-shape-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "toolkit/painters/cairo_painter.hpp"
#include "viewporter-client-protocol.h"
#include "xdg-decoration-client-protocol.h"
#include "xdg-dialog-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <EGL/egl.h>
#include <GL/gl.h>
#include <cairo.h>
#include <cmath>
#include <fcntl.h>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sys/mman.h>
#include <vector>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#include "WaylandCairoBackend.hpp"
#include "WaylandlOpenGlBackend.hpp"

#ifdef __linux__
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace toolkit {

// --- Shared memory helper ---

static int create_shm_file(size_t size) {
    int fd = -1;
#ifdef __linux__
    fd = static_cast<int>(syscall(SYS_memfd_create, "tk-shm", 1u /*MFD_CLOEXEC*/));
#endif
    if (fd < 0) {
        char name[] = "/dev/shm/tk-shm-XXXXXX";
        fd = mkstemp(name);
        if (fd >= 0) {
            unlink(name);
        }
    }
    if (fd >= 0 && ftruncate(fd, static_cast<off_t>(size)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

// --- Forward declarations for listeners ---

static void registry_global(void *data, wl_registry *reg, uint32_t name, const char *iface,
                            uint32_t version);
static void registry_global_remove(void *, wl_registry *, uint32_t) {}
static const wl_registry_listener registry_listener = {registry_global, registry_global_remove};

static void xdg_wm_base_ping(void *, xdg_wm_base *base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}
static const xdg_wm_base_listener wm_base_listener = {xdg_wm_base_ping};

static void xdg_surface_configure(void *data, xdg_surface *surf, uint32_t serial);
static const xdg_surface_listener xdg_surf_listener = {xdg_surface_configure};

static void xdg_toplevel_configure(void *data, xdg_toplevel *tl, int32_t w, int32_t h,
                                   wl_array *states);
static void xdg_toplevel_close(void *data, xdg_toplevel *tl);
static const xdg_toplevel_listener toplevel_listener = {xdg_toplevel_configure, xdg_toplevel_close};

static void pointer_enter(void *data, wl_pointer *ptr, uint32_t serial, wl_surface *surf,
                          wl_fixed_t sx, wl_fixed_t sy);
static void pointer_leave(void *data, wl_pointer *, uint32_t, wl_surface *);
static void pointer_motion(void *data, wl_pointer *, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
static void pointer_button(void *data, wl_pointer *, uint32_t serial, uint32_t time,
                           uint32_t button, uint32_t state);
static void pointer_axis(void *data, wl_pointer *, uint32_t time, uint32_t axis, wl_fixed_t value);
static void pointer_frame(void *data, wl_pointer *ptr) {}
static void pointer_axis_source(void *data, wl_pointer *ptr, uint32_t source) {}
static void pointer_axis_stop(void *data, wl_pointer *ptr, uint32_t time, uint32_t axis) {}
static void pointer_axis_discrete(void *data, wl_pointer *ptr, uint32_t axis, int32_t discrete) {}
static const wl_pointer_listener pointer_listener = {
    pointer_enter, pointer_leave,       pointer_motion,    pointer_button,       pointer_axis,
    pointer_frame, pointer_axis_source, pointer_axis_stop, pointer_axis_discrete};

static void keyboard_keymap(void *data, wl_keyboard *, uint32_t format, int32_t fd, uint32_t size);
static void keyboard_enter(void *data, wl_keyboard *, uint32_t serial, wl_surface *surf,
                           wl_array *keys);
static void keyboard_leave(void *data, wl_keyboard *, uint32_t, wl_surface *surf);
static void keyboard_key(void *data, wl_keyboard *, uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state);
static void keyboard_modifiers(void *data, wl_keyboard *, uint32_t serial, uint32_t depressed,
                               uint32_t latched, uint32_t locked, uint32_t group);
static void keyboard_repeat_info(void *data, wl_keyboard *, int32_t rate, int32_t delay) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    app->repeat_rate = rate;
    app->repeat_delay = delay;
}
static const wl_keyboard_listener keyboard_listener = {keyboard_keymap,    keyboard_enter,
                                                       keyboard_leave,     keyboard_key,
                                                       keyboard_modifiers, keyboard_repeat_info};

static void seat_capabilities(void *data, wl_seat *seat, uint32_t caps);
static void seat_name(void *, wl_seat *, const char *) {}
static const wl_seat_listener seat_listener = {seat_capabilities, seat_name};

static void frame_done(void *data, wl_callback *cb, uint32_t time);
static const wl_callback_listener frame_listener = {frame_done};

// --- wl_output listener ---

static void output_geometry(void *, wl_output *, int32_t, int32_t, int32_t, int32_t, int32_t,
                            const char *, const char *, int32_t) {}
static void output_mode(void *, wl_output *, uint32_t, int32_t, int32_t, int32_t) {}
static void output_done(void *, wl_output *) {}
static void output_scale(void *data, wl_output *, int32_t factor) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    float f = static_cast<float>(factor);
    if (f > app->output_scale) {
        app->output_scale = f;
    }
}
static const wl_output_listener output_listener = {output_geometry, output_mode, output_done,
                                                   output_scale};

static void fractional_scale_preferred_scale(void *data, wp_fractional_scale_v1 *, uint32_t scale) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);
    float f = static_cast<float>(scale) / 120.0f;
    if (std::abs(f - win->scale) > 0.01f) {
        win->scale = f;
        win->needs_redraw = true;
        win->request_redraw();
    }
}

static const wp_fractional_scale_v1_listener fractional_scale_listener = {
    fractional_scale_preferred_scale};

static void decoration_configure(void *data, zxdg_toplevel_decoration_v1 *, uint32_t mode) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);

    switch (mode) {
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
        spdlog::info("Using server-side decorations");
        break;

    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
        spdlog::warn("Compositor requested client-side decorations");
        break;

    default:
        spdlog::warn("Unknown decoration mode");
        break;
    }
}

static const zxdg_toplevel_decoration_v1_listener decoration_listener = {decoration_configure};

// --- Key mapping ---

static Key xkb_to_key(xkb_keysym_t sym) {
    switch (sym) {
    case XKB_KEY_BackSpace:
        return Key::Backspace;
    case XKB_KEY_Delete:
        return Key::Delete;
    case XKB_KEY_Left:
        return Key::Left;
    case XKB_KEY_Right:
        return Key::Right;
    case XKB_KEY_Up:
        return Key::Up;
    case XKB_KEY_Down:
        return Key::Down;
    case XKB_KEY_Home:
        return Key::Home;
    case XKB_KEY_End:
        return Key::End;
    case XKB_KEY_Page_Up:
    case XKB_KEY_KP_Page_Up:
        return Key::PageUp;
    case XKB_KEY_Page_Down:
    case XKB_KEY_KP_Page_Down:
        return Key::PageDown;
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
        return Key::Enter;
    case XKB_KEY_Escape:
        return Key::Escape;
    case XKB_KEY_Tab:
    case XKB_KEY_ISO_Left_Tab:
        return Key::Tab;
    case XKB_KEY_F1:
        return Key::F1;
    case XKB_KEY_F2:
        return Key::F2;
    case XKB_KEY_F3:
        return Key::F3;
    case XKB_KEY_F4:
        return Key::F4;
    case XKB_KEY_F5:
        return Key::F5;
    case XKB_KEY_F6:
        return Key::F6;
    case XKB_KEY_F7:
        return Key::F7;
    case XKB_KEY_F8:
        return Key::F8;
    case XKB_KEY_F9:
        return Key::F9;
    case XKB_KEY_F10:
        return Key::F10;
    case XKB_KEY_F11:
        return Key::F11;
    case XKB_KEY_F12:
        return Key::F12;
    case XKB_KEY_equal:
        return Key::Equals;
    case XKB_KEY_plus:
        return Key::Plus;
    case XKB_KEY_KP_Subtract:
        return Key::Minus;
    case XKB_KEY_1:
        return Key::Number1;
    case XKB_KEY_2:
        return Key::Number2;
    case XKB_KEY_3:
        return Key::Number3;
    case XKB_KEY_4:
        return Key::Number4;
    case XKB_KEY_5:
        return Key::Number5;
    case XKB_KEY_6:
        return Key::Number7;
    case XKB_KEY_7:
        return Key::Number7;
    case XKB_KEY_8:
        return Key::Number8;
    case XKB_KEY_9:
        return Key::Number9;
    default:
        if (128 < sym) {
            spdlog::debug("Key {} - not parsed by wayland", sym);
        }
        return Key::NoKey;
    }
}

// --- Helpers ---

static WaylandPlatformWindow *find_window(WaylandPlatformApplication *app, wl_surface *surf) {
    for (auto *w : app->windows) {
        if (w->surface == surf) {
            return w;
        }
    }
    return nullptr;
}

static const char *cursor_name_for(CursorShape shape) {
    switch (shape) {
    case CursorShape::IBeam:
        return "text";
    case CursorShape::Hand:
        return "pointer";
    case CursorShape::NotAllowed:
        return "not-allowed";
    case CursorShape::ResizeEW:
        return "ew-resize";
    case CursorShape::ResizeNS:
        return "ns-resize";
    case CursorShape::ResizeNW:
        return "nw-resize";
    case CursorShape::ResizeNESW:
        return "ne-resize";
    case CursorShape::Move:
        return "all-scroll";
    default:
        return "left_ptr";
    }
}

static wp_cursor_shape_device_v1_shape cursor_shape_for(CursorShape shape) {
    switch (shape) {
    case CursorShape::IBeam:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
    case CursorShape::Hand:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
    case CursorShape::NotAllowed:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED;
    case CursorShape::ResizeEW:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE;
    case CursorShape::ResizeNS:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE;
    case CursorShape::ResizeNW:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE;
    case CursorShape::ResizeNESW:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE;
    case CursorShape::Move:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL;
    case CursorShape::Arrow:
    default:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    }
}

static void apply_cursor(WaylandPlatformApplication *app, CursorShape shape) {
    if (!app->pointer) {
        return;
    }
    if (app->cursor_shape_device) {
        wp_cursor_shape_device_v1_set_shape(app->cursor_shape_device, app->pointer_enter_serial,
                                            cursor_shape_for(shape));
        return;
    }
    if (!app->cursor_theme || !app->cursor_surface) {
        return;
    }
    const char *name = cursor_name_for(shape);
    wl_cursor *cursor = wl_cursor_theme_get_cursor(app->cursor_theme, name);
    if (!cursor || cursor->image_count == 0) {
        return;
    }
    wl_cursor_image *img = cursor->images[0];
    wl_buffer *buf = wl_cursor_image_get_buffer(img);
    if (!buf) {
        return;
    }
    wl_surface_attach(app->cursor_surface, buf, 0, 0);
    wl_surface_damage(app->cursor_surface, 0, 0, static_cast<int32_t>(img->width),
                      static_cast<int32_t>(img->height));
    wl_surface_commit(app->cursor_surface);
    wl_pointer_set_cursor(app->pointer, app->pointer_enter_serial, app->cursor_surface,
                          static_cast<int32_t>(img->hotspot_x),
                          static_cast<int32_t>(img->hotspot_y));
}

// --- Registry ---

static void registry_global(void *data, wl_registry *reg, uint32_t name, const char *iface,
                            uint32_t version) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if (strcmp(iface, "wl_compositor") == 0) {
        app->compositor = static_cast<wl_compositor *>(
            wl_registry_bind(reg, name, &wl_compositor_interface, std::min(version, 4u)));
    } else if (strcmp(iface, "xdg_wm_base") == 0) {
        app->wm_base = static_cast<xdg_wm_base *>(
            wl_registry_bind(reg, name, &xdg_wm_base_interface, std::min(version, 2u)));
        xdg_wm_base_add_listener(app->wm_base, &wm_base_listener, app);
    } else if (strcmp(iface, "wl_shm") == 0) {
        app->shm = static_cast<wl_shm *>(wl_registry_bind(reg, name, &wl_shm_interface, 1));
    } else if (strcmp(iface, "wl_seat") == 0) {
        app->seat = static_cast<wl_seat *>(
            wl_registry_bind(reg, name, &wl_seat_interface, std::min(version, 5u)));
        wl_seat_add_listener(app->seat, &seat_listener, app);
    } else if (strcmp(iface, "wl_data_device_manager") == 0) {
        app->data_device_manager = static_cast<wl_data_device_manager *>(
            wl_registry_bind(reg, name, &wl_data_device_manager_interface, std::min(version, 3u)));
    } else if (strcmp(iface, "wl_output") == 0) {
        auto *output = static_cast<wl_output *>(
            wl_registry_bind(reg, name, &wl_output_interface, std::min(version, 2u)));
        wl_output_add_listener(output, &output_listener, app);
        app->outputs.push_back(output);
    } else if (strcmp(iface, "wp_fractional_scale_manager_v1") == 0) {
        app->fractional_scale_manager = static_cast<wp_fractional_scale_manager_v1 *>(
            wl_registry_bind(reg, name, &wp_fractional_scale_manager_v1_interface, 1));
    } else if (strcmp(iface, "wp_viewporter") == 0) {
        app->viewporter =
            static_cast<wp_viewporter *>(wl_registry_bind(reg, name, &wp_viewporter_interface, 1));
    } else if (strcmp(iface, "zxdg_decoration_manager_v1") == 0) {
        app->decoration_manager = static_cast<zxdg_decoration_manager_v1 *>(
            wl_registry_bind(reg, name, &zxdg_decoration_manager_v1_interface, 1));
    } else if (strcmp(iface, "xdg_wm_dialog_v1") == 0) {
        app->wm_dialog = static_cast<xdg_wm_dialog_v1 *>(
            wl_registry_bind(reg, name, &xdg_wm_dialog_v1_interface, 1));
    } else if (strcmp(iface, "wp_cursor_shape_manager_v1") == 0) {
        app->cursor_shape_manager = static_cast<wp_cursor_shape_manager_v1 *>(
            wl_registry_bind(reg, name, &wp_cursor_shape_manager_v1_interface, 1));
    }
}

// --- Seat ---

static void seat_capabilities(void *data, wl_seat *seat, uint32_t caps) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {
        app->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(app->pointer, &pointer_listener, app);
        if (app->cursor_shape_manager && !app->cursor_shape_device) {
            app->cursor_shape_device =
                wp_cursor_shape_manager_v1_get_pointer(app->cursor_shape_manager, app->pointer);
        }
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) {
        app->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(app->keyboard, &keyboard_listener, app);
    }
}

// --- Pointer ---

static void pointer_enter(void *data, wl_pointer *, uint32_t serial, wl_surface *surf,
                          wl_fixed_t sx, wl_fixed_t sy) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    app->pointer_enter_serial = serial;
    auto *win = find_window(app, surf);
    if (app->modal_window && win != app->modal_window) {
        app->pointer_focus = nullptr;
        return;
    }
    app->pointer_focus = win;
    app->pointer_x = static_cast<float>(wl_fixed_to_double(sx));
    app->pointer_y = static_cast<float>(wl_fixed_to_double(sy));
    if (app->pointer_focus) {
        apply_cursor(app, app->pointer_focus->current_cursor);
    }
}

static void pointer_leave(void *data, wl_pointer *, uint32_t, wl_surface *) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    app->pointer_focus = nullptr;
}

static void pointer_motion(void *data, wl_pointer *, uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    app->pointer_x = static_cast<float>(wl_fixed_to_double(sx));
    app->pointer_y = static_cast<float>(wl_fixed_to_double(sy));
    if (!app->pointer_focus) {
        return;
    }
    if (app->modal_window && app->pointer_focus != app->modal_window) {
        return;
    }
    MouseEvent e{};
    e.serial = 0; // Not available for motion
    if (app->pressed_button != -1) {
        e.type = MouseEvent::Type::Drag;
        e.button = app->pressed_button;
    } else {
        e.type = MouseEvent::Type::Move;
    }
    e.position = {app->pointer_x, app->pointer_y};
    e.shift = app->mod_shift;
    e.ctrl = app->mod_ctrl;
    e.super = app->mod_ctrl;
    app->pointer_focus->owner_->handle_mouse(e);
}

static void pointer_button(void *data, wl_pointer *, uint32_t serial, uint32_t time,
                           uint32_t button, uint32_t state) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if (!app->pointer_focus) {
        return;
    }
    if (app->modal_window && app->pointer_focus != app->modal_window) {
        return;
    }
    app->pressed_button_serial = serial;

    MouseEvent e{};

    e.serial = serial;
    e.position = {app->pointer_x, app->pointer_y};
    e.shift = app->mod_shift;
    e.ctrl = app->mod_ctrl;
    e.super = app->mod_super;

    int btn = 0;
    if (button == 0x110) {
        btn = 0; // BTN_LEFT
    } else if (button == 0x111) {
        btn = 1; // BTN_RIGHT
    } else if (button == 0x112) {
        btn = 2; // BTN_MIDDLE
    } else if (button == 0x113) {
        btn = 3; // BTN_SIDE (Back)
    } else if (button == 0x114) {
        btn = 4; // BTN_EXTRA (Forward)
    } else {
        btn = 5 + (button - 0x115);
    }

    e.button = btn;

    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        app->pressed_button = btn;
        e.type = MouseEvent::Type::Press;
        constexpr uint32_t dblclick_ms = 400;
        constexpr float dblclick_dist = 4.0f;
        bool same = (static_cast<uint32_t>(btn) == app->last_click_button);
        bool in_time = (time - app->last_click_time) < dblclick_ms;
        bool in_range = std::abs(app->pointer_x - app->last_click_x) < dblclick_dist &&
                        std::abs(app->pointer_y - app->last_click_y) < dblclick_dist;
        app->click_count = (same && in_time && in_range) ? app->click_count + 1 : 1;
        app->last_click_time = time;
        app->last_click_x = app->pointer_x;
        app->last_click_y = app->pointer_y;
        app->last_click_button = static_cast<uint32_t>(btn);
        e.click_count = app->click_count;
    } else {
        app->pressed_button = -1;
        e.type = MouseEvent::Type::Release;
    }
    app->pointer_focus->owner_->handle_mouse(e);
}

static void pointer_axis(void *data, wl_pointer *, uint32_t, uint32_t axis, wl_fixed_t value) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if (!app->pointer_focus) {
        return;
    }
    if (app->modal_window && app->pointer_focus != app->modal_window) {
        return;
    }
    MouseEvent e{};
    e.type = MouseEvent::Type::Scroll;
    e.position = {app->pointer_x, app->pointer_y};
    float v = static_cast<float>(wl_fixed_to_double(value));
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        e.scroll_dy = -v * 0.5f;
    } else {
        e.scroll_dx = v * 0.5f;
    }
    app->pointer_focus->owner_->handle_mouse(e);
}

// --- Keyboard ---

static void keyboard_keymap(void *data, wl_keyboard *, uint32_t format, int32_t fd, uint32_t size) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        ::close(fd);
        return;
    }
    char *map_str = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
    if (map_str == MAP_FAILED) {
        ::close(fd);
        return;
    }
    if (app->xkb_st) {
        xkb_state_unref(app->xkb_st);
        app->xkb_st = nullptr;
    }
    if (app->xkb_map) {
        xkb_keymap_unref(app->xkb_map);
        app->xkb_map = nullptr;
    }
    app->xkb_map = xkb_keymap_new_from_string(app->xkb_ctx, map_str, XKB_KEYMAP_FORMAT_TEXT_V1,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map_str, size);
    ::close(fd);
    if (app->xkb_map) {
        app->xkb_st = xkb_state_new(app->xkb_map);
    }
}

static void keyboard_enter(void *data, wl_keyboard *, uint32_t, wl_surface *surf, wl_array *) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    auto *win = find_window(app, surf);
    if (app->modal_window && win != app->modal_window) {
        app->keyboard_focus = nullptr;
        return;
    }
    app->keyboard_focus = win;
}

static void stop_keyboard_repeat(WaylandPlatformApplication *app) {
    if (app->repeat_timer_id) {
        for (auto it = app->timers.begin(); it != app->timers.end(); ++it) {
            if (it->id == app->repeat_timer_id) {
                app->timers.erase(it);
                break;
            }
        }
        app->repeat_timer_id = 0;
        app->repeating_key = 0;
    }
}

static void keyboard_leave(void *data, wl_keyboard *, uint32_t, wl_surface *) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if (app->keyboard_focus) {
        app->keyboard_focus->owner_->hide_tooltip();
    }
    app->keyboard_focus = nullptr;
    stop_keyboard_repeat(app);
}

static void process_key(WaylandPlatformApplication *app, uint32_t key) {
    if (!app->keyboard_focus || !app->xkb_st) {
        return;
    }
    if (app->modal_window && app->keyboard_focus != app->modal_window) {
        return;
    }
    uint32_t keycode = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_st, keycode);

    KeyEvent ke;
    ke.type = KeyEvent::Type::Press;
    ke.shift = app->mod_shift;
    ke.ctrl = app->mod_ctrl;
    ke.alt = app->mod_alt;
    ke.super = app->mod_super;
    ke.key = xkb_to_key(sym);

    if (ke.ctrl || ke.alt) {
        xkb_keysym_t base = xkb_state_key_get_one_sym(app->xkb_st, keycode);
        char buf[8] = {};
        xkb_keysym_to_utf8(base, buf, sizeof(buf));
        if (buf[0] >= 32 && static_cast<unsigned char>(buf[0]) < 127 && ke.key == Key::NoKey) {
            char lower = static_cast<char>(std::tolower(buf[0]));
            ke.text = std::string(1, lower);
        }
    } else if (ke.key == Key::NoKey) {
        char buf[8] = {};
        int len = xkb_state_key_get_utf8(app->xkb_st, keycode, buf, sizeof(buf));
        if (len > 0 && static_cast<unsigned char>(buf[0]) >= 32) {
            ke.text = std::string(buf, static_cast<size_t>(len));
        }
    }
    app->keyboard_focus->owner_->handle_key(ke);
}

static void keyboard_key(void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t key,
                         uint32_t state) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if (!app->keyboard_focus || !app->xkb_st) {
        return;
    }

    uint32_t keycode = key + 8; // Wayland keycodes are 8 units higher than XKB
    xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_st, keycode);

    KeyEvent ke;
    ke.type =
        (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? KeyEvent::Type::Press : KeyEvent::Type::Release;

    // Modifiers from keyboard_handle_modifiers
    ke.shift = app->mod_shift;
    ke.ctrl = app->mod_ctrl;
    ke.alt = app->mod_alt;
    ke.super = app->mod_super;
    ke.key = xkb_to_key(sym);

    // Get text. For Wayland, xkb_state_key_get_utf8 is usually the source.
    char buf[8] = {};
    int len = xkb_state_key_get_utf8(app->xkb_st, keycode, buf, sizeof(buf));
    if (len > 0 && static_cast<unsigned char>(buf[0]) >= 32) {
        ke.text = std::string(buf, static_cast<size_t>(len));
    } else if (ke.ctrl && sym >= XKB_KEY_a && sym <= XKB_KEY_z) {
        ke.text = std::string(1, static_cast<char>(sym));
    } else {
        ke.text = ""; // Ensure text is empty if not a printable character
    }

    // Special handling for modifier keys themselves: adjust the flag based on the event type
    if (ke.key == Key::LeftShift || ke.key == Key::RightShift) {
        ke.shift = (ke.type == KeyEvent::Type::Press);
    }
    if (ke.key == Key::LeftControl || ke.key == Key::RightControl) {
        ke.ctrl = (ke.type == KeyEvent::Type::Press);
    }
    if (ke.key == Key::LeftAlt || ke.key == Key::RightAlt) {
        ke.alt = (ke.type == KeyEvent::Type::Press);
    }
    if (ke.key == Key::LeftSuper || ke.key == Key::RightSuper) {
        ke.super = (ke.type == KeyEvent::Type::Press);
    }

    app->keyboard_focus->owner_->handle_key(ke);

    // Stop and start repeat timers only if it's a press event and not a modifier
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        bool is_modifier =
            (ke.key == Key::LeftAlt || ke.key == Key::RightAlt || ke.key == Key::LeftControl ||
             ke.key == Key::RightControl || ke.key == Key::LeftShift || ke.key == Key::RightShift ||
             ke.key == Key::LeftSuper || ke.key == Key::RightSuper);
        if (!is_modifier) {
            stop_keyboard_repeat(app);
            if (app->repeat_rate > 0) {
                app->repeating_key = key;
                float delay = static_cast<float>(app->repeat_delay) / 1000.0f;

                WaylandPlatformApplication::TimerEntry entry;
                entry.id = app->next_timer_id++;
                app->repeat_timer_id = entry.id;
                entry.interval_sec = 1.0f / static_cast<float>(app->repeat_rate);
                entry.repeats = true;
                entry.callback = [app, key] {
                    if (app->repeating_key == key) {
                        // Re-create a new KeyEvent for repeat
                        KeyEvent repeat_ke;
                        repeat_ke.type = KeyEvent::Type::Press;
                        xkb_keysym_t repeat_sym = xkb_state_key_get_one_sym(app->xkb_st, key + 8);
                        repeat_ke.key = xkb_to_key(repeat_sym);
                        repeat_ke.shift = app->mod_shift;
                        repeat_ke.ctrl = app->mod_ctrl;
                        repeat_ke.alt = app->mod_alt;
                        repeat_ke.super = app->mod_super;

                        char repeat_buf[8] = {};
                        int repeat_len = xkb_state_key_get_utf8(app->xkb_st, key + 8, repeat_buf,
                                                                sizeof(repeat_buf));
                        if (repeat_len > 0 && static_cast<unsigned char>(repeat_buf[0]) >= 32) {
                            repeat_ke.text =
                                std::string(repeat_buf, static_cast<size_t>(repeat_len));
                        } else {
                            repeat_ke.text = "";
                        }
                        app->keyboard_focus->owner_->handle_key(repeat_ke);
                    }
                };
                entry.next_fire = std::chrono::steady_clock::now() +
                                  std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                      std::chrono::duration<float>(delay));
                app->timers.push_back(std::move(entry));
            }
        }
    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (app->repeating_key == key) {
            stop_keyboard_repeat(app);
        }
    }
}

static void keyboard_modifiers(void *data, wl_keyboard *, uint32_t, uint32_t depressed,
                               uint32_t latched, uint32_t locked, uint32_t group) {
    auto *app = static_cast<WaylandPlatformApplication *>(data);
    if (!app->xkb_st) {
        return;
    }
    xkb_state_update_mask(app->xkb_st, depressed, latched, locked, 0, 0, group);
    app->mod_shift =
        xkb_state_mod_name_is_active(app->xkb_st, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE);
    app->mod_ctrl =
        xkb_state_mod_name_is_active(app->xkb_st, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE);
    app->mod_alt =
        xkb_state_mod_name_is_active(app->xkb_st, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
    app->mod_super =
        xkb_state_mod_name_is_active(app->xkb_st, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE);
}

// --- xdg_surface / xdg_toplevel ---

static void xdg_surface_configure(void *data, xdg_surface *surf, uint32_t serial) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);
    xdg_surface_ack_configure(surf, serial);
    win->configured = true;
    if (win->pending_width > 0 && win->pending_height > 0) {
        float nw = static_cast<float>(win->pending_width);
        float nh = static_cast<float>(win->pending_height);
        win->owner_->handle_resize({nw, nh});
    }
    win->needs_redraw = true;
}

static void xdg_toplevel_configure(void *data, xdg_toplevel *, int32_t w, int32_t h,
                                   wl_array *states) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);
    if (w > 0 && h > 0) {
        win->pending_width = w;
        win->pending_height = h;
    }
    auto activated = false;
    auto maximized = false;
    if (states) {
        auto *first = static_cast<uint32_t *>(states->data);
        auto count = states->size / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            if (first[i] == XDG_TOPLEVEL_STATE_ACTIVATED) {
                activated = true;
            } else if (first[i] == XDG_TOPLEVEL_STATE_MAXIMIZED) {
                maximized = true;
            }
        }
    }
    if (win->owner_) {
        if (!activated) {
            win->owner_->hide_tooltip();
        }
        win->owner_->handle_activate(activated);
        win->owner_->handle_maximized(maximized);
    }
}

static void xdg_toplevel_close(void *data, xdg_toplevel *) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);
    win->owner_->close();
}

// --- Frame callback ---

static void frame_done(void *data, wl_callback *cb, uint32_t) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);
    wl_callback_destroy(cb);
    win->frame_cb = nullptr;
    if (win->needs_redraw) {
        win->do_paint();
    }
}

static void xdg_popup_surface_configure(void *data, xdg_surface *surf, uint32_t serial) {
    xdg_surface_ack_configure(surf, serial);
}
static const xdg_surface_listener xdg_popup_surf_listener = {xdg_popup_surface_configure};

static void xdg_popup_configure(void *data, struct xdg_popup *popup, int32_t x, int32_t y,
                                int32_t width, int32_t height);
static void xdg_popup_done(void *data, struct xdg_popup *popup);
static const struct xdg_popup_listener xdg_popup_listener = {xdg_popup_configure, xdg_popup_done};

static void xdg_popup_configure(void *data, struct xdg_popup *, int32_t, int32_t, int32_t,
                                int32_t) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);
    win->needs_redraw = true;
}

static void xdg_popup_done(void *data, struct xdg_popup *) {
    auto *win = static_cast<WaylandPlatformWindow *>(data);
    win->hide_tooltip_window();
}

// --- WaylandPlatformApplication ---

WaylandPlatformApplication::WaylandPlatformApplication() {
    static CairoTextRasterizer s_rasterizer;
    set_rasterizer(&s_rasterizer);
    display = wl_display_connect(nullptr);
    if (!display) {
        throw std::runtime_error("Failed to connect to Wayland display");
    }

    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    if (!compositor || !wm_base || !shm) {
        wl_display_disconnect(display);
        display = nullptr;
        throw std::runtime_error("Wayland compositor missing required globals");
    }

    repeat_rate = 25;
    repeat_delay = 600;
    output_scale = 1;

    if (const char *env = std::getenv("SVISION_PAINT")) {
        if (std::string_view(env) == "opengl") {
            opengl_requested = true;
        }
    }

    if (opengl_requested) {
        egl_display = eglGetDisplay((EGLNativeDisplayType)display);
        if (egl_display != EGL_NO_DISPLAY) {
            EGLint major, minor;
            if (eglInitialize(egl_display, &major, &minor)) {
                EGLint attr[] = {EGL_SURFACE_TYPE,
                                 EGL_WINDOW_BIT,
                                 EGL_RED_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_ALPHA_SIZE,
                                 8,
                                 EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_BIT,
                                 EGL_NONE};
                EGLint count;
                if (eglChooseConfig(egl_display, attr, (EGLConfig *)&egl_config, 1, &count) &&
                    count > 0) {
                    eglBindAPI(EGL_OPENGL_API);
                    EGLint ctx_attr[] = {EGL_NONE};
                    egl_context =
                        eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, ctx_attr);
                }
            }
        }
        if (!egl_context) {
            spdlog::warn("Failed to initialize EGL, falling back to Cairo");
            opengl_requested = false;
        }
    }

    int cursor_size = 24;
    if (const char *env_size = std::getenv("XCURSOR_SIZE")) {
        int s = std::atoi(env_size);
        if (s > 0) {
            cursor_size = s;
        }
    } else {
        cursor_size = static_cast<int>(24.0f * output_scale);
    }
    const char *cursor_theme_name = std::getenv("XCURSOR_THEME");
    cursor_theme = wl_cursor_theme_load(cursor_theme_name, cursor_size, shm);
    if (compositor) {
        cursor_surface = wl_compositor_create_surface(compositor);
    }

    if (pipe(wakeup_pipe) == 0) {
        fcntl(wakeup_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(wakeup_pipe[1], F_SETFL, O_NONBLOCK);
    }

    if (decoration_manager) {
        spdlog::info("xdg-decoration protocol available");
    } else {
        spdlog::warn("xdg-decoration protocol NOT available");
    }
    wl_registry_destroy(registry);
    spdlog::debug("Wayland backend initialized (scale={}, opengl={})", output_scale,
                  opengl_requested);
}

WaylandPlatformApplication::~WaylandPlatformApplication() {
    if (xkb_st) {
        xkb_state_unref(xkb_st);
    }
    if (xkb_map) {
        xkb_keymap_unref(xkb_map);
    }
    if (xkb_ctx) {
        xkb_context_unref(xkb_ctx);
    }
    for (auto *o : outputs) {
        wl_output_destroy(o);
    }
    if (cursor_surface) {
        wl_surface_destroy(cursor_surface);
    }
    if (cursor_theme) {
        wl_cursor_theme_destroy(cursor_theme);
    }
    if (pointer) {
        wl_pointer_destroy(pointer);
    }
    if (keyboard) {
        wl_keyboard_destroy(keyboard);
    }
    if (data_device) {
        wl_data_device_destroy(data_device);
    }
    if (seat) {
        wl_seat_destroy(seat);
    }
    if (cursor_shape_device) {
        wp_cursor_shape_device_v1_destroy(cursor_shape_device);
    }
    if (cursor_shape_manager) {
        wp_cursor_shape_manager_v1_destroy(cursor_shape_manager);
    }
    if (wm_dialog) {
        xdg_wm_dialog_v1_destroy(wm_dialog);
    }
    if (decoration_manager) {
        zxdg_decoration_manager_v1_destroy(decoration_manager);
    }
    if (viewporter) {
        wp_viewporter_destroy(viewporter);
    }
    if (fractional_scale_manager) {
        wp_fractional_scale_manager_v1_destroy(fractional_scale_manager);
    }
    if (data_device_manager) {
        wl_data_device_manager_destroy(data_device_manager);
    }
    if (wm_base) {
        xdg_wm_base_destroy(wm_base);
    }
    if (shm) {
        wl_shm_destroy(shm);
    }
    if (compositor) {
        wl_compositor_destroy(compositor);
    }
    if (display) {
        if (egl_display) {
            if (egl_context) {
                eglDestroyContext(egl_display, egl_context);
            }
            eglTerminate(egl_display);
        }
        wl_display_disconnect(display);
    }
    if (wakeup_pipe[0] >= 0) {
        ::close(wakeup_pipe[0]);
    }
    if (wakeup_pipe[1] >= 0) {
        ::close(wakeup_pipe[1]);
    }
}

std::unique_ptr<PlatformWindow> WaylandPlatformApplication::create_window(std::string_view title,
                                                                          Size size, Window *owner,
                                                                          WindowOptions options) {
    return std::make_unique<WaylandPlatformWindow>(this, title, size, owner, options);
}

std::shared_ptr<ImageLoaderInterface> WaylandPlatformApplication::get_image_loader() {
    if (!image_loader_) {
        image_loader_ = std::make_shared<StbImageLoader>();
    }
    return image_loader_;
}

std::shared_ptr<SVGLoaderInterface> WaylandPlatformApplication::get_svg_loader() {
    if (!svg_loader_) {
        svg_loader_ = std::make_shared<LunasvgImageLoader>();
    }
    return svg_loader_;
}

// Runs one iteration of the Wayland event loop. Returns false when the loop should stop.
static bool wl_run_iteration(WaylandPlatformApplication *app) {
    // 1. Process timers and posted functions.
    std::vector<std::function<void()>> to_call;
    {
        auto now = std::chrono::steady_clock::now();
        for (auto it = app->timers.begin(); it != app->timers.end();) {
            if (now >= it->next_fire) {
                to_call.push_back(it->callback);
                if (it->repeats) {
                    it->next_fire =
                        now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                  std::chrono::duration<float>(it->interval_sec));
                    ++it;
                } else {
                    it = app->timers.erase(it);
                }
            } else {
                ++it;
            }
        }
    }
    {
        std::lock_guard lock(app->posted_mutex);
        for (auto &fn : app->posted_fns) {
            to_call.push_back(std::move(fn));
        }
        app->posted_fns.clear();
    }
    for (auto &fn : to_call) {
        fn();
    }

    // 2. Redraw windows that need it.
    for (auto *win : app->windows) {
        if (win->configured && win->needs_redraw && !win->frame_cb) {
            win->do_paint();
        }
    }

    // 3. Flush the display.
    wl_display_flush(app->display);

    // 4. Calculate poll timeout from next timer.
    int timeout_ms = -1;
    if (!app->timers.empty()) {
        auto now = std::chrono::steady_clock::now();
        for (auto &t : app->timers) {
            auto ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(t.next_fire - now).count();
            if (ms < 0) {
                ms = 0;
            }
            if (timeout_ms < 0 || ms < timeout_ms) {
                timeout_ms = static_cast<int>(ms);
            }
        }
    }

    // 5. Poll for events.
    int wl_fd = wl_display_get_fd(app->display);
    struct pollfd fds[2];
    fds[0] = {wl_fd, POLLIN, 0};
    fds[1] = {app->wakeup_pipe[0], POLLIN, 0};
    poll(fds, 2, timeout_ms);

    if (fds[1].revents & POLLIN) {
        char buf[64];
        while (::read(app->wakeup_pipe[0], buf, sizeof(buf)) > 0) {
        }
    }

    // 6. Dispatch events.
    if (fds[0].revents & POLLIN) {
        wl_display_dispatch(app->display);
    } else {
        wl_display_dispatch_pending(app->display);
    }

    if (app->windows.empty()) {
        app->running = false;
    }

    return app->running;
}

int WaylandPlatformApplication::run() {
    running = true;
    spdlog::info("Starting run loop (Wayland)");
    while (wl_run_iteration(this)) {
    }
    return 0;
}

void WaylandPlatformApplication::run_until(std::function<bool()> should_exit) {
    while (!should_exit() && wl_run_iteration(this)) {
    }
}

void WaylandPlatformApplication::quit() { running = false; }

void WaylandPlatformApplication::post_to_main_thread(std::function<void()> fn) {
    {
        std::lock_guard lock(posted_mutex);
        posted_fns.push_back(std::move(fn));
    }
    char c = 1;
    (void)::write(wakeup_pipe[1], &c, 1);
}

std::string WaylandPlatformApplication::clipboard_get_text() {
    // TODO: implement via wl_data_device/wl_data_offer for cross-client paste
    return clipboard_content;
}

void WaylandPlatformApplication::clipboard_set_text(std::string const &text) {
    clipboard_content = text;
    // TODO: implement via wl_data_source for cross-client copy
}

// --- WaylandPlatformWindow ---

WaylandPlatformWindow::WaylandPlatformWindow(WaylandPlatformApplication *app,
                                             std::string_view title, Size size, Window *owner,
                                             WindowOptions options)
    : app_(app), owner_(owner) {

    surface = wl_compositor_create_surface(app_->compositor);
    xdg_surface_ = xdg_wm_base_get_xdg_surface(app_->wm_base, surface);
    xdg_surface_add_listener(xdg_surface_, &xdg_surf_listener, this);
    xdg_toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
    xdg_toplevel_add_listener(xdg_toplevel_, &toplevel_listener, this);

    if (app_->decoration_manager) {
        toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
            app_->decoration_manager, xdg_toplevel_);

        static const zxdg_toplevel_decoration_v1_listener decoration_listener = {
            // configure
            [](void *data, zxdg_toplevel_decoration_v1 *decoration, uint32_t mode) {
                auto *win = static_cast<WaylandPlatformWindow *>(data);

                switch (mode) {
                case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
                    spdlog::info("Wayland compositor supports server-side decorations");
                    break;

                case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
                    spdlog::info("Wayland compositor using client-side decorations");
                    break;

                default:
                    spdlog::warn("Unknown decoration mode");
                    break;
                }
            }};

        zxdg_toplevel_decoration_v1_add_listener(toplevel_decoration, &decoration_listener, this);

        auto mode = (options.csd || options.frameless)
                        ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
                        : ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        zxdg_toplevel_decoration_v1_set_mode(toplevel_decoration, mode);
    }
    std::string t(title);
    xdg_toplevel_set_title(xdg_toplevel_, t.c_str());
    std::string app_id = "svision3";
    if (Application::has_instance()) {
        app_id = Application::instance().application_name();
    }
    xdg_toplevel_set_app_id(xdg_toplevel_, app_id.c_str());

    pending_width = static_cast<int>(size.width);
    pending_height = static_cast<int>(size.height);
    scale = app_->output_scale;

    if (app_->fractional_scale_manager) {
        fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
            app_->fractional_scale_manager, surface);
        wp_fractional_scale_v1_add_listener(fractional_scale, &fractional_scale_listener, this);
    }
    if (app_->viewporter) {
        viewport = wp_viewporter_get_viewport(app_->viewporter, surface);
    }

    if (app_->opengl_requested) {
        backend = std::make_unique<WaylandlOpenGlBackend>(surface, size, app_);
    } else {
        backend = std::make_unique<WaylandCairoBackend>();
    }

    wl_surface_set_buffer_scale(surface, app_->output_scale);
    wl_surface_commit(surface);
    wl_display_roundtrip(app_->display);
    app_->windows.push_back(this);
}

WaylandPlatformWindow::~WaylandPlatformWindow() {
    auto &w = app_->windows;
    w.erase(std::remove(w.begin(), w.end(), this), w.end());
    if (app_->pointer_focus == this) {
        app_->pointer_focus = nullptr;
    }
    if (app_->keyboard_focus == this) {
        app_->keyboard_focus = nullptr;
    }
    if (frame_cb) {
        wl_callback_destroy(frame_cb);
    }
    if (buffer) {
        wl_buffer_destroy(buffer);
    }
    if (shm_data && shm_size > 0) {
        munmap(shm_data, shm_size);
    }
    if (shm_fd >= 0) {
        ::close(shm_fd);
    }
    if (xdg_dialog) {
        xdg_dialog_v1_destroy(xdg_dialog);
    }
    if (xdg_toplevel_) {
        xdg_toplevel_destroy(xdg_toplevel_);
    }
    if (toplevel_decoration) {
        zxdg_toplevel_decoration_v1_destroy(toplevel_decoration);
    }
    if (fractional_scale) {
        wp_fractional_scale_v1_destroy(fractional_scale);
    }
    if (viewport) {
        wp_viewport_destroy(viewport);
    }
    if (xdg_surface_) {
        xdg_surface_destroy(xdg_surface_);
    }
    if (egl_surface) {
        eglDestroySurface(app_->egl_display, egl_surface);
    }
    if (egl_window) {
        wl_egl_window_destroy(egl_window);
    }
    if (surface) {
        wl_surface_destroy(surface);
    }
    hide_tooltip_window();
}

void WaylandPlatformWindow::show() {
    wl_surface_commit(surface);
    wl_display_roundtrip(app_->display);
}

void WaylandPlatformWindow::close() {
    auto &w = app_->windows;
    w.erase(std::remove(w.begin(), w.end(), this), w.end());
    if (app_->pointer_focus == this) {
        app_->pointer_focus = nullptr;
    }
    if (app_->keyboard_focus == this) {
        app_->keyboard_focus = nullptr;
    }
    if (app_->modal_window == this) {
        app_->modal_window = nullptr;
        app_->modal_parent = nullptr;
    }
    if (frame_cb) {
        wl_callback_destroy(frame_cb);
        frame_cb = nullptr;
    }
    if (xdg_dialog) {
        xdg_dialog_v1_destroy(xdg_dialog);
        xdg_dialog = nullptr;
    }
    if (toplevel_decoration) {
        zxdg_toplevel_decoration_v1_destroy(toplevel_decoration);
        toplevel_decoration = nullptr;
    }
    if (xdg_toplevel_) {
        xdg_toplevel_destroy(xdg_toplevel_);
        xdg_toplevel_ = nullptr;
    }
    if (fractional_scale) {
        wp_fractional_scale_v1_destroy(fractional_scale);
        fractional_scale = nullptr;
    }
    if (viewport) {
        wp_viewport_destroy(viewport);
        viewport = nullptr;
    }
    if (xdg_surface_) {
        xdg_surface_destroy(xdg_surface_);
        xdg_surface_ = nullptr;
    }
    if (surface) {
        wl_surface_destroy(surface);
        surface = nullptr;
    }
    wl_display_flush(app_->display);
}

void WaylandPlatformWindow::minimize() {
    if (xdg_toplevel_) {
        xdg_toplevel_set_minimized(xdg_toplevel_);
    }
}

void WaylandPlatformWindow::maximize() {
    if (xdg_toplevel_) {
        xdg_toplevel_set_maximized(xdg_toplevel_);
    }
}

void WaylandPlatformWindow::restore() {
    if (xdg_toplevel_) {
        xdg_toplevel_unset_maximized(xdg_toplevel_);
    }
}

void WaylandPlatformWindow::set_size(Size s) {
    // In Wayland, the client doesn't really "resize" itself for top-level surfaces.
    // However, we can update our internal state and trigger a redraw.
    pending_width = static_cast<int>(s.width);
    pending_height = static_cast<int>(s.height);
    request_redraw();
}

void WaylandPlatformWindow::request_redraw() { needs_redraw = true; }

void WaylandPlatformWindow::set_min_size(Size s) {
    if (xdg_toplevel_) {
        xdg_toplevel_set_min_size(xdg_toplevel_, static_cast<int32_t>(s.width),
                                  static_cast<int32_t>(s.height));
    }
}

void WaylandPlatformWindow::set_max_size(Size s) {
    if (xdg_toplevel_ && s.width > 0 && s.height > 0) {
        xdg_toplevel_set_max_size(xdg_toplevel_, static_cast<int32_t>(s.width),
                                  static_cast<int32_t>(s.height));
    }
}

int WaylandPlatformWindow::start_timer(float interval_sec, std::function<void()> callback,
                                       bool repeats) {
    int tid = app_->next_timer_id++;
    WaylandPlatformApplication::TimerEntry entry;
    entry.id = tid;
    entry.interval_sec = interval_sec;
    entry.repeats = repeats;
    entry.callback = std::move(callback);
    entry.next_fire = std::chrono::steady_clock::now() +
                      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<float>(interval_sec));
    app_->timers.push_back(std::move(entry));
    return tid;
}

void WaylandPlatformWindow::stop_timer(int timer_id) {
    auto &timers = app_->timers;
    timers.erase(std::remove_if(timers.begin(), timers.end(),
                                [timer_id](auto &t) { return t.id == timer_id; }),
                 timers.end());
}

void WaylandPlatformWindow::set_title(std::string_view t) {
    xdg_toplevel_set_title(xdg_toplevel_, std::string(t).c_str());
}

void WaylandPlatformWindow::set_cursor(CursorShape shape) {
    if (current_cursor == shape) {
        return;
    }
    current_cursor = shape;
    apply_cursor(app_, shape);
}

void WaylandPlatformWindow::start_system_move(uint32_t serial) {
    app_->pressed_button = -1;
    xdg_toplevel_move(xdg_toplevel_, app_->seat, serial);
}

void WaylandPlatformWindow::start_system_resize(WindowEdge edge, uint32_t serial) {
    app_->pressed_button = -1;
    uint32_t edges = 0;
    switch (edge) {
    case WindowEdge::Top:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
        break;
    case WindowEdge::Bottom:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
        break;
    case WindowEdge::Left:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
        break;
    case WindowEdge::Right:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
        break;
    case WindowEdge::TopLeft:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
        break;
    case WindowEdge::TopRight:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
        break;
    case WindowEdge::BottomLeft:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
        break;
    case WindowEdge::BottomRight:
        edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
        break;
    default:
        return;
    }
    xdg_toplevel_resize(xdg_toplevel_, app_->seat, serial, edges);
}
void WaylandPlatformWindow::show_tooltip_window(std::string const &text, Point pos) {
    if (!xdg_surface_ || !configured) {
        return;
    }
    if (tooltip_data && tooltip_data->text == text) {
        return;
    }
    hide_tooltip_window();
    tooltip_data = std::make_unique<TooltipData>();
    tooltip_data->text = text;

    auto const &theme = Theme::current();
    auto const &style = Theme::current().style.tooltip;
    auto fs = theme.palette.fonts.size;
    auto *r = app_->rasterizer();
    auto tsz = r ? r->measure(text, fs) : Size{};
    // FIXME: padding for tooltip is hardcoded in wayland.
    auto padding = 5.0f;
    auto fm = r ? r->metrics(fs) : Painter::FontMetrics{};
    auto tw = tsz.width + padding * 2;
    auto th = fm.height + padding * 2;

    tooltip_data->width = static_cast<int>(std::ceil(tw * scale));
    tooltip_data->height = static_cast<int>(std::ceil(th * scale));

    tooltip_data->surface = wl_compositor_create_surface(app_->compositor);
    tooltip_data->xdg_surf = xdg_wm_base_get_xdg_surface(app_->wm_base, tooltip_data->surface);
    xdg_surface_add_listener(tooltip_data->xdg_surf, &xdg_popup_surf_listener, this);

    struct xdg_positioner *pos_obj = xdg_wm_base_create_positioner(app_->wm_base);
    xdg_positioner_set_size(pos_obj, static_cast<int32_t>(std::ceil(tw)),
                            static_cast<int32_t>(std::ceil(th)));
    xdg_positioner_set_anchor_rect(pos_obj, static_cast<int32_t>(std::round(pos.x)),
                                   static_cast<int32_t>(std::round(pos.y)), 1, 1);
    xdg_positioner_set_gravity(pos_obj, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    xdg_positioner_set_anchor(pos_obj, XDG_POSITIONER_ANCHOR_TOP_LEFT);
    xdg_positioner_set_offset(pos_obj, 0, 10);

    tooltip_data->popup = xdg_surface_get_popup(tooltip_data->xdg_surf, xdg_surface_, pos_obj);
    xdg_popup_add_listener(tooltip_data->popup, &xdg_popup_listener, this);
    xdg_positioner_destroy(pos_obj);

    if (app_->viewporter) {
        tooltip_data->viewport =
            wp_viewporter_get_viewport(app_->viewporter, tooltip_data->surface);
        wp_viewport_set_destination(tooltip_data->viewport, static_cast<int>(std::ceil(tw)),
                                    static_cast<int>(std::ceil(th)));
        wl_surface_set_buffer_scale(tooltip_data->surface, 1);
    } else {
        wl_surface_set_buffer_scale(tooltip_data->surface, static_cast<int32_t>(std::ceil(scale)));
    }

    wl_surface_commit(tooltip_data->surface);
    wl_display_roundtrip(app_->display);
    needs_redraw = true;
}

void WaylandPlatformWindow::set_modal_for(PlatformWindow *parent) {
    auto *p = static_cast<WaylandPlatformWindow *>(parent);
    if (xdg_toplevel_ && p && p->xdg_toplevel_) {
        xdg_toplevel_set_parent(xdg_toplevel_, p->xdg_toplevel_);
    }
    if (app_->wm_dialog && xdg_toplevel_ && !xdg_dialog) {
        xdg_dialog = xdg_wm_dialog_v1_get_xdg_dialog(app_->wm_dialog, xdg_toplevel_);
        xdg_dialog_v1_set_modal(xdg_dialog);
    }
    app_->modal_window = this;
    app_->modal_parent = p;
    if (app_->keyboard_focus && app_->keyboard_focus != this) {
        app_->keyboard_focus = nullptr;
    }
    if (app_->pointer_focus && app_->pointer_focus != this) {
        app_->pointer_focus = nullptr;
    }
}

void WaylandPlatformWindow::hide_tooltip_window() {
    if (!tooltip_data) {
        return;
    }

    if (tooltip_data->popup) {
        xdg_popup_destroy(tooltip_data->popup);
    }
    if (tooltip_data->viewport) {
        wp_viewport_destroy(tooltip_data->viewport);
    }
    if (tooltip_data->xdg_surf) {
        xdg_surface_destroy(tooltip_data->xdg_surf);
    }
    if (tooltip_data->buffer) {
        wl_buffer_destroy(tooltip_data->buffer);
    }
    if (tooltip_data->shm_data) {
        munmap(tooltip_data->shm_data, tooltip_data->shm_size);
    }
    if (tooltip_data->shm_fd >= 0) {
        ::close(tooltip_data->shm_fd);
    }
    if (tooltip_data->surface) {
        wl_surface_destroy(tooltip_data->surface);
    }

    tooltip_data.reset();
}

void WaylandPlatformWindow::do_paint() {
    needs_redraw = false;
    int lw = static_cast<int>(owner_->size().width);
    int lh = static_cast<int>(owner_->size().height);
    if (lw <= 0 || lh <= 0) {
        return;
    }

    if (frame_cb) {
        wl_callback_destroy(frame_cb);
    }
    frame_cb = wl_surface_frame(surface);
    wl_callback_add_listener(frame_cb, &frame_listener, this);

    if (backend) {
        backend->paint(owner_, this, app_, lw, lh);
    }

    if (tooltip_data && tooltip_data->surface) {
        paint_tooltip();
    }
}

// FIXME: this function draws using cairo - need to use RenderingBackend for indirection
void WaylandPlatformWindow::paint_tooltip() {
    if (!tooltip_data || !tooltip_data->surface) {
        return;
    }

    int pw = tooltip_data->width;
    int ph = tooltip_data->height;
    if (pw <= 0 || ph <= 0) {
        return;
    }

    int stride = pw * 4;
    size_t total = static_cast<size_t>(stride) * static_cast<size_t>(ph);
    int fd = create_shm_file(total);
    if (fd < 0) {
        return;
    }

    void *data = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        ::close(fd);
        return;
    }

    auto const &palette = Theme::current().palette;
    auto fs = palette.fonts.size;
    auto tw = static_cast<float>(pw) / scale;
    auto th = static_cast<float>(ph) / scale;
    auto padding = 5.0f;

    backend->render_to_buffer(app_, pw, ph, scale, data, [&](Painter &p) {
        auto fm = p.font_metrics(fs);
        Rect r{0, 0, tw, th};
        p.fill_rounded_rect(r, palette.tooltip, Theme::current().style.corner_radius);
        p.draw_rounded_rect(r, palette.border, Theme::current().style.corner_radius, Theme::current().style.border_width);
        p.draw_text(tooltip_data->text, {padding, padding + fm.ascent}, palette.text, fs);
    });

    wl_shm_pool *pool = wl_shm_create_pool(app_->shm, fd, static_cast<int32_t>(total));
    wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, pw, ph, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    wl_surface_attach(tooltip_data->surface, buf, 0, 0);
    wl_surface_damage(tooltip_data->surface, 0, 0, pw, ph);
    wl_surface_commit(tooltip_data->surface);

    wl_display_roundtrip(app_->display);

    munmap(data, total);
    ::close(fd);
    wl_buffer_destroy(buf);
}

bool WaylandPlatformWindow::save_to_png(std::string const &path) {
    return cairo_save_to_png(owner_, path);
}

float WaylandPlatformWindow::scale_factor() const { return static_cast<float>(app_->output_scale); }

float WaylandPlatformApplication::scale_factor() const { return output_scale; }

SystemFonts WaylandPlatformApplication::system_fonts() const {
    return linux_utils::detect_system_fonts();
}

} // namespace toolkit
