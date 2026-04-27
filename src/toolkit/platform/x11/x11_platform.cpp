// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "x11_platform.hpp"
#include "../linux_utils.hpp"
#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/painters/gl_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <spdlog/spdlog.h>

#undef None
#undef CursorShape

#include <cmath>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "X11CairoBackend.hpp"
#include "X11OpenGlBackend.hpp"

namespace toolkit {

// ── Impl structs ────────────────────────────────────────────────────────────

struct X11PlatformApplication::Impl {
    Display *display = nullptr;
    int screen = 0;
    ::Window root = 0L;
    Visual *visual = nullptr;
    int depth = 0;
    Colormap colormap = 0;
    Visual *argb_visual = nullptr;
    int argb_depth = 0;
    Colormap argb_colormap = 0;
    float scale = 1.0f;
    bool running = false;
    Atom wm_delete_window, wm_protocols, net_wm_name, utf8_string;
    Atom clipboard_atom, targets_atom, tk_sel;
    Atom motif_wm_hints;
    XIM xim = nullptr;

    struct WindowData {
        Window *owner = nullptr;
        XIC xic = nullptr;
        Time last_press_time = 0;
        int last_press_x = 0, last_press_y = 0;
        int click_count = 0;
    };
    std::unordered_map<::Window, WindowData> window_map;

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
    ::Window clipboard_owner_window = 0L;
    bool opengl_requested = false;
    ::Window modal_xwindow = 0L;
};

struct X11PlatformWindow::Impl {
    ::Window xwindow = 0L;
    ::Window tooltip_xwindow = 0L;
    Cursor arrow_cursor = 0L, ibeam_cursor = 0L;
    Cursor hand_cursor = 0L, not_allowed_cursor = 0L;
    std::unique_ptr<RenderingBackend> backend;

    bool needs_redraw = false;
    bool is_modal = false;
    bool has_input_grab = false;
};

// ── Helpers ─────────────────────────────────────────────────────────────────

static float detect_x11_scale(Display *display) {
    if (const char *env = std::getenv("GDK_SCALE")) {
        float s = std::strtof(env, nullptr);
        if (s > 0) {
            return s;
        }
    }
    if (const char *env = std::getenv("QT_SCALE_FACTOR")) {
        float s = std::strtof(env, nullptr);
        if (s > 0) {
            return s;
        }
    }
    XrmInitialize();
    char *rms = XResourceManagerString(display);
    if (rms) {
        XrmDatabase db = XrmGetStringDatabase(rms);
        if (db) {
            XrmValue value;
            char *type = nullptr;
            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value)) {
                float dpi = std::strtof(value.addr, nullptr);
                XrmDestroyDatabase(db);
                if (dpi > 0) {
                    return dpi / 96.0f;
                }
            }
            XrmDestroyDatabase(db);
        }
    }
    return 1.0f;
}

struct MwmHints {
    enum Flags {
        Functions = 1 << 0,
        Decorations = 1 << 1,
        InputMode = 1 << 2,
        State = 1 << 3,
    };
    enum {
        All = 0xFFFFFFFF,
        None = 0,
    };
    unsigned long flags = 0;
    unsigned long functions = 0;
    unsigned long decorations = 0;
    long input_mode = 0;
    unsigned long state = 0;
};

static void set_motif_hints(::Window window, Display *display, Atom motif_wm_hints,
                            bool decorated) {
    MwmHints hints;
    hints.flags = MwmHints::Decorations;
    hints.decorations = decorated ? MwmHints::All : MwmHints::None;
    XChangeProperty(display, window, motif_wm_hints, motif_wm_hints, 32, PropModeReplace,
                    reinterpret_cast<unsigned char const *>(&hints), 5);
}

static Key keysym_to_key(KeySym ks) {
    switch (ks) {
    case XK_BackSpace:
        return Key::Backspace;
    case XK_Delete:
        return Key::Delete;
    case XK_Left:
        return Key::Left;
    case XK_Right:
        return Key::Right;
    case XK_Up:
        return Key::Up;
    case XK_Down:
        return Key::Down;
    case XK_Home:
        return Key::Home;
    case XK_End:
        return Key::End;
    case XK_Page_Up:
    case XK_KP_Page_Up:
        return Key::PageUp;
    case XK_Page_Down:
    case XK_KP_Page_Down:
        return Key::PageDown;
    case XK_Return:
    case XK_KP_Enter:
        return Key::Enter;
    case XK_Escape:
        return Key::Escape;
    case XK_Tab:
    case XK_ISO_Left_Tab:
        return Key::Tab;
    case XK_F1:
        return Key::F1;
    case XK_F2:
        return Key::F2;
    case XK_F3:
        return Key::F3;
    case XK_F4:
        return Key::F4;
    case XK_F5:
        return Key::F5;
    case XK_F6:
        return Key::F6;
    case XK_F7:
        return Key::F7;
    case XK_F8:
        return Key::F8;
    case XK_F9:
        return Key::F9;
    case XK_F10:
        return Key::F10;
    case XK_F11:
        return Key::F11;
    case XK_F12:
        return Key::F12;
    case XK_Alt_L:
        return Key::LeftAlt;
    case XK_Alt_R:
        return Key::RightAlt;
    case XK_Shift_L:
        return Key::LeftShift;
    case XK_Shift_R:
        return Key::RightShift;
    case XK_Control_L:
        return Key::LeftControl;
    case XK_Control_R:
        return Key::RightControl;
    case XK_Super_L:
        return Key::LeftSuper;
    case XK_Super_R:
        return Key::RightSuper;
    default:
        return Key::NoKey;
    }
}

static int detect_click_count(X11PlatformApplication::Impl::WindowData &data, XButtonEvent &btn) {
    constexpr Time double_click_ms = 400;
    constexpr auto double_click_dist = 4;
    auto in_time = (btn.time - data.last_press_time) < double_click_ms;
    auto in_range = std::abs(btn.x - data.last_press_x) < double_click_dist &&
                    std::abs(btn.y - data.last_press_y) < double_click_dist;
    data.click_count = (in_time && in_range) ? data.click_count + 1 : 1;
    data.last_press_time = btn.time;
    data.last_press_x = btn.x;
    data.last_press_y = btn.y;
    return data.click_count;
}

static void dispatch_x11_event(X11PlatformApplication::Impl *app, ::Window xwin,
                               X11PlatformApplication::Impl::WindowData &data, XEvent &xev) {
    auto win = data.owner;
    auto xwin_plat = static_cast<X11PlatformWindow *>(win->platform_window());
    auto w = xwin_plat->impl_.get();

    switch (xev.type) {
    case Expose: {
        if (xev.xexpose.count != 0) {
            break;
        }
        w->needs_redraw = true;
        break;
    }
    case ConfigureNotify: {
        float scale = app->scale;
        float nw = static_cast<float>(xev.xconfigure.width) / scale;
        float nh = static_cast<float>(xev.xconfigure.height) / scale;
        if (nw != win->size().width || nh != win->size().height) {
            win->handle_resize({nw, nh});
            w->needs_redraw = true;
        }
        break;
    }
    case ButtonPress: {
        auto &btn = xev.xbutton;
        auto scale = app->scale;
        auto pos = Point{static_cast<float>(btn.x) / scale, static_cast<float>(btn.y) / scale};
        if (btn.button >= 4 && btn.button <= 7) {
            auto e = MouseEvent{};
            e.type = MouseEvent::Type::Scroll;
            e.position = pos;

            // FIXME: scroll offset should from from the platform, not hardcoded
            switch (btn.button) {
            case 4:
                e.scroll_dy = 20.0f;
                break;
            case 5:
                e.scroll_dy = -20.0f;
                break;
            case 6:
                e.scroll_dx = -20.0f;
                break;
            case 7:
                e.scroll_dx = 20.0f;
                break;
            }
            win->handle_mouse(e);
            break;
        }

        auto e = MouseEvent{};
        e.type = MouseEvent::Type::Press;
        e.position = pos;
        if (btn.button == Button1) {
            e.button = 0;
        } else if (btn.button == Button3) {
            e.button = 1;
        } else if (btn.button == Button2) {
            e.button = 2;
        } else if (btn.button == 8) {
            e.button = 3;
        } else if (btn.button == 9) {
            e.button = 4;
        } else {
            e.button = 5;
        }
        e.click_count = detect_click_count(data, btn);
        e.shift = (btn.state & ShiftMask) != 0;
        e.ctrl = (btn.state & ControlMask) != 0;
        e.super = (btn.state & Mod4Mask) != 0;
        win->handle_mouse(e);
        break;
    }
    case ButtonRelease: {
        auto &btn = xev.xbutton;
        auto scale = app->scale;
        if (btn.button >= 4 && btn.button <= 7) {
            break;
        }
        auto e = MouseEvent{};
        e.type = MouseEvent::Type::Release;
        e.position = {static_cast<float>(btn.x) / scale, static_cast<float>(btn.y) / scale};
        if (btn.button == Button1) {
            e.button = 0;
        } else if (btn.button == Button3) {
            e.button = 1;
        } else if (btn.button == Button2) {
            e.button = 2;
        } else if (btn.button == 8) {
            e.button = 3;
        } else if (btn.button == 9) {
            e.button = 4;
        } else {
            e.button = 5;
        }
        win->handle_mouse(e);
        break;
    }
    case MotionNotify: {
        auto &m = xev.xmotion;
        MouseEvent e{};
        auto scale = app->scale;
        auto held = (m.state & (Button1Mask | Button2Mask | Button3Mask)) != 0;

        e.type = held ? MouseEvent::Type::Drag : MouseEvent::Type::Move;
        e.position = {static_cast<float>(m.x) / scale, static_cast<float>(m.y) / scale};
        win->handle_mouse(e);
        break;
    }
    case KeyPress: {
        auto &key = xev.xkey;
        auto st = key.state;
        auto len = 0;
        char buf[64];
        KeySym keysym = NoSymbol;

        KeyEvent ke;
        ke.type = KeyEvent::Type::Press;

        if (data.xic) {
            Status status;
            len = Xutf8LookupString(data.xic, &key, buf, sizeof(buf) - 1, &keysym, &status);
        } else {
            len = XLookupString(&key, buf, sizeof(buf) - 1, &keysym, nullptr);
        }

        // Set modifier flags based on current state (st)
        ke.shift = (st & ShiftMask) != 0;
        ke.ctrl = (st & ControlMask) != 0;
        ke.alt = (st & Mod1Mask) != 0;
        ke.super = (st & Mod4Mask) != 0;

        // Adjust modifier flags if the key pressed IS a modifier key
        if (keysym == XK_Shift_L || keysym == XK_Shift_R) {
            ke.shift = true;
        }
        if (keysym == XK_Control_L || keysym == XK_Control_R) {
            ke.ctrl = true;
        }
        if (keysym == XK_Alt_L || keysym == XK_Alt_R) {
            ke.alt = true;
        }
        if (keysym == XK_Super_L || keysym == XK_Super_R) {
            ke.super = true;
        }

        if (len > 0 && static_cast<unsigned char>(buf[0]) >= 32) {
            ke.text.assign(buf, len);
        }
        ke.key = keysym_to_key(keysym);

        win->handle_key(ke);
        break;
    }
    case KeyRelease: {
        auto &key = xev.xkey;
        KeyEvent ke;
        ke.type = KeyEvent::Type::Release;
        unsigned st = key.state;
        KeySym keysym = NoSymbol;
        char buf[64] = {};
        int len = 0;

        if (data.xic) {
            Status status;
            len = Xutf8LookupString(data.xic, &key, buf, sizeof(buf) - 1, &keysym, &status);
        } else {
            len = XLookupString(&key, buf, sizeof(buf) - 1, &keysym, nullptr);
        }

        ke.shift = (st & ShiftMask) != 0;
        ke.ctrl = (st & ControlMask) != 0;
        ke.alt = (st & Mod1Mask) != 0;
        ke.super = (st & Mod4Mask) != 0;

        if (keysym == XK_Shift_L || keysym == XK_Shift_R) {
            ke.shift = false;
        }
        if (keysym == XK_Control_L || keysym == XK_Control_R) {
            ke.ctrl = false;
        }
        if (keysym == XK_Alt_L || keysym == XK_Alt_R) {
            ke.alt = false;
        }
        if (keysym == XK_Super_L || keysym == XK_Super_R) {
            ke.super = false;
        }

        if (len > 0 && static_cast<unsigned char>(buf[0]) >= 32) {
            ke.text.assign(buf, len);
        }
        ke.key = keysym_to_key(keysym);
        win->handle_key(ke);
        break;
    }
    case ClientMessage:
        if (xev.xclient.message_type == app->wm_protocols &&
            static_cast<Atom>(xev.xclient.data.l[0]) == app->wm_delete_window) {
            win->close();
        }
        break;
    case FocusOut:
        win->hide_tooltip();
        break;
    default:
        break;
    }
}

static ::Window extract_event_window(XEvent &ev) {
    switch (ev.type) {
    case Expose:
        return ev.xexpose.window;
    case ConfigureNotify:
        return ev.xconfigure.window;
    case ButtonPress:
    case ButtonRelease:
        return ev.xbutton.window;
    case MotionNotify:
        return ev.xmotion.window;
    case KeyPress:
    case KeyRelease:
        return ev.xkey.window;
    case ClientMessage:
        return ev.xclient.window;
    case FocusIn:
    case FocusOut:
        return ev.xfocus.window;
    default:
        return 0L;
    }
}

static void handle_selection_request(X11PlatformApplication::Impl *d, XSelectionRequestEvent &req) {
    XSelectionEvent resp = {};
    resp.type = SelectionNotify;
    resp.requestor = req.requestor;
    resp.selection = req.selection;
    resp.target = req.target;
    resp.time = req.time;
    resp.property = 0L;
    if (req.target == d->targets_atom) {
        Atom sup[] = {d->targets_atom, d->utf8_string, XA_STRING};
        XChangeProperty(d->display, req.requestor, req.property, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(sup), 3);
        resp.property = req.property;
    } else if (req.target == d->utf8_string || req.target == XA_STRING) {
        XChangeProperty(d->display, req.requestor, req.property, req.target, 8, PropModeReplace,
                        reinterpret_cast<unsigned char const *>(d->clipboard_content.data()),
                        static_cast<int>(d->clipboard_content.size()));
        resp.property = req.property;
    }
    XSendEvent(d->display, req.requestor, False, 0, reinterpret_cast<XEvent *>(&resp));
}

static void process_pending_events(X11PlatformApplication::Impl *d) {
    while (XPending(d->display)) {
        XEvent ev;
        XNextEvent(d->display, &ev);
        if (ev.type == SelectionRequest) {
            handle_selection_request(d, ev.xselectionrequest);
            continue;
        }
        if (ev.type == SelectionNotify || ev.type == SelectionClear) {
            continue;
        }
        auto xw = extract_event_window(ev);
        if (xw == 0L) {
            continue;
        }
        if (d->modal_xwindow != 0L && xw != d->modal_xwindow)
            continue;
        auto it = d->window_map.find(xw);
        if (it != d->window_map.end()) {
            dispatch_x11_event(d, xw, it->second, ev);
        }
    }
}

// ── X11PlatformApplication ──────────────────────────────────────────────────

X11PlatformApplication::X11PlatformApplication() : impl_(std::make_unique<Impl>()) {
    static CairoTextRasterizer s_rasterizer;
    set_rasterizer(&s_rasterizer);
    auto *d = impl_.get();
    XInitThreads();
    d->display = XOpenDisplay(nullptr);
    if (!d->display) {
        spdlog::error("Failed to open X11 display");
        return;
    }
    d->screen = DefaultScreen(d->display);
    d->root = RootWindow(d->display, d->screen);
    d->visual = DefaultVisual(d->display, d->screen);
    d->depth = DefaultDepth(d->display, d->screen);
    d->colormap = DefaultColormap(d->display, d->screen);

    XVisualInfo vinfo;
    if (XMatchVisualInfo(d->display, d->screen, 32, TrueColor, &vinfo)) {
        d->argb_visual = vinfo.visual;
        d->argb_depth = vinfo.depth;
        d->argb_colormap = XCreateColormap(d->display, d->root, d->argb_visual, AllocNone);
    }

    d->wm_delete_window = XInternAtom(d->display, "WM_DELETE_WINDOW", False);
    d->wm_protocols = XInternAtom(d->display, "WM_PROTOCOLS", False);
    d->net_wm_name = XInternAtom(d->display, "_NET_WM_NAME", False);
    d->utf8_string = XInternAtom(d->display, "UTF8_STRING", False);
    d->clipboard_atom = XInternAtom(d->display, "CLIPBOARD", False);
    d->targets_atom = XInternAtom(d->display, "TARGETS", False);
    d->tk_sel = XInternAtom(d->display, "TK_SELECTION", False);
    d->motif_wm_hints = XInternAtom(d->display, "_MOTIF_WM_HINTS", False);
    XSetLocaleModifiers("");
    d->xim = XOpenIM(d->display, nullptr, nullptr, nullptr);
    if (!d->xim) {
        XSetLocaleModifiers("@im=none");
        d->xim = XOpenIM(d->display, nullptr, nullptr, nullptr);
    }
    d->scale = detect_x11_scale(d->display);
    if (pipe(d->wakeup_pipe) == 0) {
        fcntl(d->wakeup_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(d->wakeup_pipe[1], F_SETFL, O_NONBLOCK);
    }

    if (auto env = std::getenv("SVISION_PAINT")) {
        if (std::string_view(env) == "opengl") {
            d->opengl_requested = true;
        }
    }

    spdlog::debug("X11 backend initialized (scale={:.2f}, opengl={})", d->scale,
                  d->opengl_requested);
}

void *X11PlatformApplication::get_display() const { return impl_->display; }
void *X11PlatformApplication::get_visual() const { return impl_->visual; }

X11PlatformApplication::~X11PlatformApplication() {
    auto *d = impl_.get();
    if (d->argb_colormap) {
        XFreeColormap(d->display, d->argb_colormap);
    }
    if (d->xim) {
        XCloseIM(d->xim);
    }
    if (d->display) {
        XCloseDisplay(d->display);
    }
    if (d->wakeup_pipe[0] >= 0) {
        ::close(d->wakeup_pipe[0]);
    }
    if (d->wakeup_pipe[1] >= 0) {
        ::close(d->wakeup_pipe[1]);
    }
}

std::unique_ptr<PlatformWindow> X11PlatformApplication::create_window(std::string_view title,
                                                                      Size size, Window *owner) {
    return std::make_unique<X11PlatformWindow>(this, title, size, owner);
}

// Runs one iteration of the event loop. Returns false when the loop should stop.
static bool x11_run_iteration(X11PlatformApplication::Impl *d) {
    if (d->window_map.empty()) {
        d->running = false;
        return false;
    }

    // 1. Process timers and posted functions.
    std::vector<std::function<void()>> to_call;
    {
        auto const now = std::chrono::steady_clock::now();
        for (auto it = d->timers.begin(); it != d->timers.end();) {
            if (now >= it->next_fire) {
                to_call.push_back(it->callback);
                if (it->repeats) {
                    it->next_fire =
                        now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                  std::chrono::duration<float>(it->interval_sec));
                    ++it;
                } else {
                    it = d->timers.erase(it);
                }
            } else {
                ++it;
            }
        }
    }
    {
        std::lock_guard lock(d->posted_mutex);
        for (auto &fn : d->posted_fns)
            to_call.push_back(std::move(fn));
        d->posted_fns.clear();
    }
    for (auto const &fn : to_call)
        fn();

    // 2. Redraw windows that need it.
    {
        std::vector<X11PlatformWindow *> active_windows;
        for (auto &pair : d->window_map)
            active_windows.push_back(
                static_cast<X11PlatformWindow *>(pair.second.owner->platform_window()));
        for (auto *plat : active_windows) {
            if (d->window_map.count(plat->impl_->xwindow) && plat->impl_->needs_redraw)
                plat->do_paint();
        }
    }

    if (!d->running)
        return false;

    // 3. Calculate poll timeout from next timer.
    int timeout_ms = -1;
    if (!d->timers.empty()) {
        auto const now = std::chrono::steady_clock::now();
        for (auto const &t : d->timers) {
            auto const ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(t.next_fire - now).count();
            int const wait_ms = ms < 0 ? 0 : static_cast<int>(ms);
            if (timeout_ms < 0 || wait_ms < timeout_ms)
                timeout_ms = wait_ms;
        }
    }

    // 4. Poll for events.
    struct pollfd fds[2];
    fds[0] = {ConnectionNumber(d->display), POLLIN, 0};
    fds[1] = {d->wakeup_pipe[0], POLLIN, 0};

    if (XPending(d->display))
        timeout_ms = 0;

    XFlush(d->display);
    poll(fds, 2, timeout_ms);

    if (fds[1].revents & POLLIN) {
        char buf[64];
        while (::read(d->wakeup_pipe[0], buf, sizeof(buf)) > 0) {
        }
    }

    // 5. Dispatch events.
    process_pending_events(d);
    return d->running;
}

int X11PlatformApplication::run() {
    auto *d = impl_.get();
    d->running = true;
    spdlog::info("Starting run loop (X11)");
    while (x11_run_iteration(d)) {}
    return 0;
}

void X11PlatformApplication::run_until(std::function<bool()> should_exit) {
    auto *d = impl_.get();
    while (!should_exit() && x11_run_iteration(d)) {}
}

void X11PlatformApplication::quit() { impl_->running = false; }

void X11PlatformApplication::post_to_main_thread(std::function<void()> fn) {
    auto *d = impl_.get();
    {
        std::lock_guard lock(d->posted_mutex);
        d->posted_fns.push_back(std::move(fn));
    }
    char c = 1;
    (void)::write(d->wakeup_pipe[1], &c, 1);
}

std::string X11PlatformApplication::clipboard_get_text() {
    auto *d = impl_.get();
    if (!d->display) {
        return {};
    }
    ::Window owner = XGetSelectionOwner(d->display, d->clipboard_atom);
    if (owner == d->clipboard_owner_window && owner != 0L) {
        return d->clipboard_content;
    }
    if (d->window_map.empty()) {
        return {};
    }
    ::Window receiver = d->window_map.begin()->first;
    XConvertSelection(d->display, d->clipboard_atom, d->utf8_string, d->tk_sel, receiver,
                      CurrentTime);
    XFlush(d->display);
    XEvent ev;
    for (int i = 0; i < 50; ++i) {
        if (XCheckTypedWindowEvent(d->display, receiver, SelectionNotify, &ev)) {
            break;
        }
        struct timespec ts = {0, 20'000'000};
        nanosleep(&ts, nullptr);
    }
    if (ev.type != SelectionNotify || ev.xselection.property == 0L) {
        return {};
    }
    Atom at;
    int af;
    unsigned long ni, bl;
    unsigned char *data = nullptr;
    XGetWindowProperty(d->display, receiver, d->tk_sel, 0, 1 << 20, True, AnyPropertyType, &at, &af,
                       &ni, &bl, &data);
    std::string result;
    if (data && ni > 0) {
        result.assign(reinterpret_cast<char *>(data), ni);
    }
    if (data) {
        XFree(data);
    }
    return result;
}

void X11PlatformApplication::clipboard_set_text(std::string const &text) {
    auto *d = impl_.get();
    if (!d->display) {
        return;
    }
    d->clipboard_content = text;
    if (d->window_map.empty()) {
        return;
    }
    ::Window owner = d->window_map.begin()->first;
    d->clipboard_owner_window = owner;
    XSetSelectionOwner(d->display, d->clipboard_atom, owner, CurrentTime);
    XFlush(d->display);
}


X11PlatformWindow::X11PlatformWindow(X11PlatformApplication *app, std::string_view title, Size size,
                                     Window *owner)
    : impl_(std::make_unique<Impl>()), app_(app), owner_(owner) {
    auto *d = app_->impl_.get();
    auto *w = impl_.get();

    Visual *visual = d->visual;
    int depth = d->depth;
    Colormap colormap = d->colormap;
    GLXContext glx_context = nullptr;

    if (d->opengl_requested) {
        GLint att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, 0};
        XVisualInfo *vi = glXChooseVisual(d->display, d->screen, att);
        if (vi) {
            visual = vi->visual;
            depth = vi->depth;
            colormap = XCreateColormap(d->display, d->root, visual, AllocNone);
            glx_context = glXCreateContext(d->display, vi, nullptr, GL_TRUE);
            XFree(vi);
        } else {
            spdlog::warn("Failed to choose GLX visual, falling back to Cairo");
        }
    }

    XSetWindowAttributes swa = {};
    swa.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | KeyPressMask | KeyReleaseMask | FocusChangeMask;
    swa.colormap = colormap;
    swa.background_pixmap = 0L; // None
    swa.bit_gravity = StaticGravity;
    swa.backing_store = WhenMapped;

    // Use theme background color for initial window background to avoid black blink
    Color const &bg = Theme::current().palette.window;
    swa.background_pixel = (static_cast<unsigned long>(bg.r * 255) << 16) |
                           (static_cast<unsigned long>(bg.g * 255) << 8) |
                           (static_cast<unsigned long>(bg.b * 255));

    float scale = d->scale;
    w->xwindow = XCreateWindow(
        d->display, d->root, 0, 0, static_cast<unsigned>(size.width * scale),
        static_cast<unsigned>(size.height * scale), 0, depth, InputOutput, visual,
        CWEventMask | CWColormap | CWBackPixmap | CWBitGravity | CWBackingStore | CWBackPixel,
        &swa);

    if (glx_context) {
        w->backend = std::make_unique<X11OpenGlBackend>(w->xwindow, glx_context);
    } else {
        w->backend = std::make_unique<X11CairoBackend>(w->xwindow);
    }
    std::string t(title);
    XStoreName(d->display, w->xwindow, t.c_str());
    XChangeProperty(d->display, w->xwindow, d->net_wm_name, d->utf8_string, 8, PropModeReplace,
                    reinterpret_cast<unsigned char const *>(t.c_str()), static_cast<int>(t.size()));
    XSetWMProtocols(d->display, w->xwindow, &d->wm_delete_window, 1);
    XIC xic = nullptr;
    if (d->xim) {
        xic = XCreateIC(d->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
                        w->xwindow, XNFocusWindow, w->xwindow, nullptr);
    }
    w->arrow_cursor = XCreateFontCursor(d->display, XC_left_ptr);
    w->ibeam_cursor = XCreateFontCursor(d->display, XC_xterm);
    w->hand_cursor = XCreateFontCursor(d->display, XC_hand2);
    w->not_allowed_cursor = XCreateFontCursor(d->display, XC_X_cursor);
    d->window_map[w->xwindow] = {owner, xic};

    impl_->needs_redraw = true;
}

X11PlatformWindow::~X11PlatformWindow() { cleanup_resources(); }

void X11PlatformWindow::cleanup_resources() {
    auto *d = app_->impl_.get();
    auto *w = impl_.get();

    if (w->xwindow == 0L) {
        return;
    }

    // Backend handles cairo surfaces and GLX context cleanup
    w->backend.reset();

    auto it = d->window_map.find(w->xwindow);
    if (it != d->window_map.end()) {
        if (it->second.xic) {
            XDestroyIC(it->second.xic);
        }
        d->window_map.erase(it);
    }

    if (w->arrow_cursor) {
        XFreeCursor(d->display, w->arrow_cursor);
        w->arrow_cursor = 0L;
    }
    if (w->ibeam_cursor) {
        XFreeCursor(d->display, w->ibeam_cursor);
        w->ibeam_cursor = 0L;
    }
    if (w->hand_cursor) {
        XFreeCursor(d->display, w->hand_cursor);
        w->hand_cursor = 0L;
    }
    if (w->not_allowed_cursor) {
        XFreeCursor(d->display, w->not_allowed_cursor);
        w->not_allowed_cursor = 0L;
    }
    if (w->tooltip_xwindow != 0L) {
        XDestroyWindow(d->display, w->tooltip_xwindow);
        w->tooltip_xwindow = 0L;
    }

    if (w->has_input_grab) {
        XUngrabPointer(d->display, CurrentTime);
        XUngrabKeyboard(d->display, CurrentTime);
        w->has_input_grab = false;
    }

    if (d->modal_xwindow == w->xwindow)
        d->modal_xwindow = 0L;

    XDestroyWindow(d->display, w->xwindow);
    w->xwindow = 0L;
}

void X11PlatformWindow::show() {
    auto *display = static_cast<Display *>(app_->impl_->display);
    XMapRaised(display, impl_->xwindow);
    // Force first paint immediately after mapping to avoid blink
    if (impl_->needs_redraw) {
        do_paint();
    }
    XFlush(display);

    if (impl_->is_modal && !impl_->has_input_grab) {
        XGrabPointer(display, impl_->xwindow, True,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ButtonMotionMask,
                     GrabModeAsync, GrabModeAsync, 0L, 0L, CurrentTime);
        XGrabKeyboard(display, impl_->xwindow, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        impl_->has_input_grab = true;
    }
}

void X11PlatformWindow::set_modal_for(PlatformWindow *parent) {
    auto *d = app_->impl_.get();
    auto *p = static_cast<X11PlatformWindow *>(parent);
    XSetTransientForHint(d->display, impl_->xwindow, p->impl_->xwindow);

    Atom net_wm_state = XInternAtom(d->display, "_NET_WM_STATE", False);
    Atom net_wm_modal = XInternAtom(d->display, "_NET_WM_STATE_MODAL", False);
    XChangeProperty(d->display, impl_->xwindow, net_wm_state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char *>(&net_wm_modal), 1);

    Atom net_wm_window_type = XInternAtom(d->display, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_window_type_dialog = XInternAtom(d->display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(d->display, impl_->xwindow, net_wm_window_type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char *>(&net_wm_window_type_dialog), 1);

    impl_->is_modal = true;
    app_->impl_->modal_xwindow = impl_->xwindow;
}

void X11PlatformWindow::close() { cleanup_resources(); }

void X11PlatformWindow::minimize() {
    auto *d = app_->impl_.get();
    XIconifyWindow(d->display, impl_->xwindow, d->screen);
    XFlush(d->display);
}

void X11PlatformWindow::maximize() {
    auto *d = app_->impl_.get();
    Atom net_wm_state = XInternAtom(d->display, "_NET_WM_STATE", False);
    Atom net_wm_max_horz = XInternAtom(d->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom net_wm_max_vert = XInternAtom(d->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    XEvent event = {};
    event.type = ClientMessage;
    event.xclient.window = impl_->xwindow;
    event.xclient.message_type = net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = net_wm_max_horz;
    event.xclient.data.l[2] = net_wm_max_vert;
    event.xclient.data.l[3] = 0;
    XSendEvent(d->display, d->root, False, SubstructureNotifyMask | SubstructureRedirectMask,
               &event);
    XFlush(d->display);
}

void X11PlatformWindow::set_size(Size s) {
    float scale = scale_factor();
    XResizeWindow(static_cast<Display *>(app_->impl_->display), impl_->xwindow,
                  static_cast<unsigned int>(std::max(1.0f, s.width * scale)),
                  static_cast<unsigned int>(std::max(1.0f, s.height * scale)));
}

void X11PlatformWindow::request_redraw() { impl_->needs_redraw = true; }

void X11PlatformWindow::do_paint() {
    if (!impl_->needs_redraw || impl_->xwindow == 0L || !impl_->backend) {
        return;
    }
    impl_->needs_redraw = false;

    int lw = static_cast<int>(owner_->size().width);
    int lh = static_cast<int>(owner_->size().height);
    impl_->backend->paint(owner_, this, app_, lw, lh);
}

void X11PlatformWindow::set_min_size(Size s) {
    auto *d = app_->impl_.get();
    auto *w = impl_.get();
    auto const scale = d->scale;
    auto hints = XSizeHints{};
    auto supplied = 0L;

    XGetWMNormalHints(d->display, w->xwindow, &hints, &supplied);
    if (s.width > 0 && s.height > 0) {
        hints.flags |= PMinSize;
        hints.min_width = static_cast<int>(s.width * scale);
        hints.min_height = static_cast<int>(s.height * scale);
    } else {
        hints.flags &= ~PMinSize;
    }

    auto const mx = owner_->max_size();
    if (mx.width > 0 && mx.height > 0) {
        hints.flags |= PMaxSize;
        hints.max_width = static_cast<int>(mx.width * scale);
        hints.max_height = static_cast<int>(mx.height * scale);
    }
    XSetWMNormalHints(d->display, w->xwindow, &hints);
    XFlush(d->display);
}

void X11PlatformWindow::set_max_size(Size s) {
    auto *d = app_->impl_.get();
    auto *w = impl_.get();
    auto const scale = d->scale;
    auto hints = XSizeHints{};
    auto supplied = 0L;

    XGetWMNormalHints(d->display, w->xwindow, &hints, &supplied);
    if (s.width > 0 && s.height > 0) {
        hints.flags |= PMaxSize;
        hints.max_width = static_cast<int>(s.width * scale);
        hints.max_height = static_cast<int>(s.height * scale);
    } else {
        hints.flags &= ~PMaxSize;
    }

    auto const mn = owner_->min_size();
    if (mn.width > 0 || mn.height > 0) {
        hints.flags |= PMinSize;
        hints.min_width = static_cast<int>(mn.width * scale);
        hints.min_height = static_cast<int>(mn.height * scale);
    }
    XSetWMNormalHints(d->display, w->xwindow, &hints);
    XFlush(d->display);
}

int X11PlatformWindow::start_timer(float interval_sec, std::function<void()> callback,
                                   bool repeats) {
    auto *d = app_->impl_.get();
    auto tid = d->next_timer_id++;
    auto entry = X11PlatformApplication::Impl::TimerEntry{};

    entry.id = tid;
    entry.interval_sec = interval_sec;
    entry.repeats = repeats;
    entry.callback = std::move(callback);
    entry.next_fire = std::chrono::steady_clock::now() +
                      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<float>(interval_sec));
    d->timers.push_back(std::move(entry));
    return tid;
}

void X11PlatformWindow::stop_timer(int timer_id) {
    auto &timers = app_->impl_->timers;
    timers.erase(std::remove_if(timers.begin(), timers.end(),
                                [timer_id](auto &t) { return t.id == timer_id; }),
                 timers.end());
}

void X11PlatformWindow::set_cursor(CursorShape shape) {
    auto *w = impl_.get();
    Cursor c;
    switch (shape) {
    case CursorShape::IBeam:
        c = w->ibeam_cursor;
        break;
    case CursorShape::Hand:
        c = w->hand_cursor;
        break;
    case CursorShape::NotAllowed:
        c = w->not_allowed_cursor;
        break;
    default:
        c = w->arrow_cursor;
        break;
    }
    XDefineCursor(static_cast<Display *>(app_->impl_->display), w->xwindow, c);
}

void X11PlatformWindow::show_tooltip_window(std::string const &text, Point local_pos) {
    auto *d = app_->impl_.get();
    auto *w = impl_.get();
    auto scale = d->scale;
    auto const &style = Theme::current().tooltip;
    auto const &theme = Theme::current();
    // FIXME padding for tooltips is hardcodede in X11
    auto pad = 5.0f;
    auto fs = theme.palette.fonts.size;
    auto *r = app_->rasterizer();
    auto tsz = r ? r->measure(text, fs) : Size{};
    auto fm = r ? r->metrics(fs) : Painter::FontMetrics{};
    auto tw = tsz.width + pad * 2, th = fm.height + pad * 2;

    int sx, sy;
    ::Window child;
    XTranslateCoordinates(d->display, w->xwindow, d->root, static_cast<int>(local_pos.x * scale),
                          static_cast<int>(local_pos.y * scale), &sx, &sy, &child);
    auto piw = std::max(1, static_cast<int>(std::ceil(tw * scale)));
    auto pih = std::max(1, static_cast<int>(std::ceil(th * scale)));
    auto screen_w = DisplayWidth(d->display, d->screen);
    auto visual = d->argb_visual ? d->argb_visual : d->visual;
    auto depth = d->argb_visual ? d->argb_depth : d->depth;
    auto colormap = d->argb_visual ? d->argb_colormap : d->colormap;

    sy -= pih + 4;
    if (sx + piw > screen_w) {
        sx = screen_w - piw - 2;
    }
    if (sx < 0) {
        sx = 2;
    }
    if (sy < 0) {
        sy = sy + pih + 24;
    }

    if (w->tooltip_xwindow == 0L) {
        XSetWindowAttributes sa = {};
        sa.override_redirect = True;
        sa.save_under = True;
        sa.colormap = colormap;
        sa.border_pixel = 0;
        sa.background_pixel = 0;
        w->tooltip_xwindow = XCreateWindow(
            d->display, d->root, sx, sy, piw, pih, 0, depth, InputOutput, visual,
            CWOverrideRedirect | CWSaveUnder | CWColormap | CWBorderPixel | CWBackPixel, &sa);
    } else {
        XMoveResizeWindow(d->display, w->tooltip_xwindow, sx, sy, piw, pih);
    }
    XMapRaised(d->display, w->tooltip_xwindow);

    std::vector<uint8_t> pixels(static_cast<size_t>(piw) * pih * 4);
    w->backend->render_to_buffer(app_, piw, pih, scale, pixels.data(), [&](Painter &p) {
        auto fm = p.font_metrics(fs);
        Rect r{0, 0, tw, th};
        p.fill_rounded_rect(r, theme.palette.tooltip, theme.palette.corner_radius);
        p.draw_rounded_rect(r, theme.palette.border, theme.palette.corner_radius,
                            theme.palette.border_width);
        p.draw_text(text, {pad, pad + fm.ascent}, theme.palette.text, fs);
    });

    GC gc = XCreateGC(d->display, w->tooltip_xwindow, 0, nullptr);
    XImage *img = XCreateImage(d->display, static_cast<Visual *>(visual), depth, ZPixmap, 0,
                               reinterpret_cast<char *>(pixels.data()), piw, pih, 32, piw * 4);
    XPutImage(d->display, w->tooltip_xwindow, gc, img, 0, 0, 0, 0, piw, pih);
    img->data = nullptr;
    XDestroyImage(img);
    XFreeGC(d->display, gc);
    XFlush(d->display);
}

void X11PlatformWindow::hide_tooltip_window() {

    auto *w = impl_.get();
    if (w->tooltip_xwindow != 0L) {
        XUnmapWindow(static_cast<Display *>(app_->impl_->display), w->tooltip_xwindow);
        XFlush(static_cast<Display *>(app_->impl_->display));
    }
}

bool X11PlatformWindow::save_to_png(std::string const &path) {
    return cairo_save_to_png(owner_, path);
}

float X11PlatformWindow::scale_factor() const { return app_->impl_->scale; }

std::string_view X11PlatformWindow::painter_name() const { return impl_->backend->name(); }

float X11PlatformApplication::scale_factor() const { return impl_->scale; }

SystemFonts X11PlatformApplication::system_fonts() const {
    return linux_utils::detect_system_fonts();
}

} // namespace toolkit
