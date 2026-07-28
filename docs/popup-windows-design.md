# Popups as Native Sub-Windows

Scoping doc, not an implementation plan: what it would take to turn `Menu`,
`ContextMenu`, and `Combobox`'s dropdowns from software overlays painted
inside the owning window into real OS-level popup windows. Prompted by
fixing text clipping in the current overlay popups (`window.cpp`'s popup
paint loop now clips to `popup.bounds`); that fix covers the common case but
can't cover a popup that legitimately needs to extend past the edge of its
owning window.

Status markers: `[exists]` already in the tree and reusable as-is,
`[gap]` exists but doesn't do enough for this, needs extension, `[new]` not
implemented.

## 1. Current architecture

`Menu::show`, `ContextMenu`'s equivalent, and `Combobox`'s dropdown
(`menu.cpp:107-120`, `context_menu.cpp:88-93`, `combobox.cpp:114-134`) all
build the same `Popup` value —

```cpp
// window.hpp
struct Popup {
    Rect bounds;
    std::function<void(Painter &)> on_paint;
    std::function<bool(MouseEvent const &)> on_mouse;
    std::function<bool(KeyEvent const &)> on_key;
    std::function<void()> on_close;
};
```

— and hand it to `Window::open_popup`, which pushes it onto `popups_`
(`window.cpp:578-585`). Every frame, `Window::handle_paint` clips and
translates the shared `Painter` to each popup's bounds and calls
`on_paint` (`window.cpp:711-718`); every mouse/key event is walked down the
`popups_` stack top-first and handed to `on_mouse`/`on_key`
(`window.cpp:861-904`), with a `click_inside_any_popup` check closing
everything if the click landed outside all of them. Nested submenus
(`Menu::open_submenu`, `menu.cpp:365-395`) just push another `Popup` onto
the same stack, positioned relative to the parent popup's `bounds_` in the
same window-local coordinate space.

This means a popup is, structurally, just pixels drawn inside the *owning*
`Window`'s own native surface. `Menu::show` computes and clamps its
position against `window_->size()` (`menu.cpp:88-102`) because that's the
only canvas it has. Consequences:

- it can't extend past the edge of the owning OS window — a menu opened
  near the edge of a small or maximized window has nowhere to go but to
  get clamped/overlap awkwardly, it can never spill onto another monitor
  or off the window's own bounds
- it can't get an independent OS-level drop shadow, blur, or rounded-corner
  compositing from the window manager/compositor
- it can't float above *other applications'* windows — it's still just
  content inside our one window's surface, subject to that window's own
  stacking position
- it's simple and fast, and works correctly (including the clipping just
  added) for the overwhelmingly common case: a menu near a button, inside
  a normal-sized window

## 2. Two existing precedents to build from

Two things already in the codebase get most of the way to a real popup
window, each missing a different half of what a menu needs.

### 2.1 `[exists → gap]` `show_tooltip_window` — real popup surface, no input

Every backend already creates a genuine, borderless, always-on-top native
window for tooltips, does monitor-edge-aware positioning, and renders a
`Painter`'s output into it:

| Backend | Mechanism | Reference |
|---|---|---|
| X11 | `override_redirect` `XCreateWindow` + `XPutImage` | `x11_platform.cpp:1648-1680` |
| Wayland | `xdg_surface_get_popup` + `xdg_positioner` | `wl_platform.cpp:1717-1746` |
| Win32 | `WS_POPUP` + `WS_EX_LAYERED` + `UpdateLayeredWindow` | `win32_platform.cpp:1358-1400` |
| macOS | borderless `NSWindow`, `NSStatusWindowLevel` | `macos_native_platform.mm:772-795` |

What's missing for a menu:

- every one of these explicitly opts *out* of input: X11/Win32 never call
  `XSelectInput`/register a window proc for input, Wayland's tooltip popup
  never calls `xdg_popup_grab`, and macOS sets
  `setIgnoresMouseEvents:YES`. A menu needs pointer motion (hover
  highlight), press/release (item activation), and keyboard (arrow nav,
  mnemonics, Escape) all routed back into `Menu::handle_mouse`/`handle_key`.
- rendering is one-shot: painted once via `RenderingBackend::render_to_buffer`
  when the tooltip appears. A menu repaints on every hover/press change.
- singleton per `PlatformWindow` (`tooltip_xwindow`, `tooltip_data`,
  `tooltip_hwnd`, `tooltip_window` — one field, reused/moved). A menu stack
  needs arbitrary depth for nested submenus.
- the monitor-edge-clamping math each backend already does for tooltip
  placement is exactly the math a real popup window needs — this part is
  directly reusable.

### 2.2 `[exists → gap]` `Application::create_window` + `frameless` + `set_modal_for` — real interactive secondary window

`MessageBox` and `FileDialog` already create a full second `toolkit::Window`
with `WindowOptions{.frameless = true}` and mark it modal
(`message_box.cpp:56,143,168,250`). This proves the multi-window path is
already live in production: one `PlatformApplication` already pumps events
for N native windows / N `toolkit::Window`s concurrently, today.

What's missing for a menu:

- it's a *full* `toolkit::Window` — root widget, layout, its own min/max
  size handling, etc. A menu wants something much lighter: no root layout,
  just "a rect that paints one thing and forwards input."
- `set_modal_for` blocks interaction with the parent until the child
  closes. A menu instead wants dismiss-on-outside-click (an implicit
  grab), not modality — the parent window must stay interactive around it.
- no anchor-relative positioning helper — dialogs are positioned once
  (typically centered) and stay there; a menu is positioned relative to
  whatever opened it (a button, a menu item) plus screen-edge clamping.

## 3. What real popup windows would require

### 3.1 `[new]` A lightweight popup-window factory in the platform layer

```cpp
// platform.hpp
virtual std::unique_ptr<PlatformWindow> create_popup(PlatformWindow *parent,
                                                      Rect screen_rect) = 0;
```

Modeled on the tooltip-window creation code in §2.1, which already solves
surface creation, backend selection (Cairo vs. GL variants — each platform
has both), and scale-factor handling. Kept separate from
`create_window`/`WindowOptions` rather than adding a popup flag there, so
`PlatformWindow`'s full window contract (resize, minimize, decorations,
...) doesn't need `[[no-op if popup]]` carve-outs sprinkled through every
backend.

### 3.2 `[new]` Per-backend: promote the tooltip window from passive to interactive

For each backend, the work is turning the existing one-shot, input-ignoring
tooltip window (§2.1) into one that forwards events and repaints on demand:

- **X11**: same `override_redirect` `XCreateWindow`, but add
  `XSelectInput` for `ButtonPress`/`ButtonRelease`/`PointerMotion`/`KeyPress`,
  and register the new `xwindow` in whatever table the main event loop uses
  to resolve `Window*` from an X window id (tooltip windows never needed
  this since they never receive events).
- **Wayland**: `xdg_popup` already supports this by protocol — call
  `xdg_popup_grab(popup, seat, serial)` at creation for an implicit
  pointer/keyboard grab from the compositor, and handle the `popup_done`
  event as "user clicked outside, please close." This is the cleanest of
  the four platforms; the protocol was designed for exactly this case, and
  it replaces the manual outside-click math entirely.
- **Win32**: same `WS_POPUP` + `WS_EX_LAYERED` window, but drop
  `WS_EX_NOACTIVATE` (needed so it can receive keyboard input) and add a
  window proc; dismissal comes from `WM_ACTIVATE`/`WM_KILLFOCUS` on the
  popup instead of an explicit grab.
- **macOS**: same borderless `NSWindow`, but `ignoresMouseEvents` flips to
  `NO`, and it should be a non-activating panel (`NSPanel` with
  `becomesKeyOnlyIfNeeded`, or an `NSWindow` plus a local `NSEvent` monitor
  for clicks outside its frame) so it can accept clicks/keys without
  stealing focus from the owning window in the app switcher/menu bar.

### 3.3 `[gap]` Rendering: reuse `render_to_buffer`, just call it more often

`RenderingBackend::render_to_buffer` already exists per backend × renderer
variant (Cairo + OpenGL, four platforms) and is exactly what tooltip
windows use for their single paint. A popup window needs the same call
issued again on every hover/press-state change and re-blitted the same way
tooltips already do (`XPutImage` / `UpdateLayeredWindow` /
`wl_surface_commit` / `setNeedsDisplay`) — no new rendering machinery, just
a higher call frequency and a real input loop driving it.

### 3.4 `[new]` Outside-click / focus-loss dismissal — four different idioms, not one

Today this is one piece of shared code (`window.cpp:891-904`,
`click_inside_any_popup`) because everything is one window's event stream.
Once popups are separate native windows, "close on outside click" has no
single cross-platform answer:

| Backend | Mechanism |
|---|---|
| X11 | keep an active `XGrabPointer` for the popup's lifetime (`grab_pointer()` already exists, `window.cpp:252-256`) so every `ButtonPress` anywhere is delivered to the popup's own handler; compare screen coords against the popup stack |
| Wayland | `xdg_popup`'s `popup_done` (§3.2) — no manual coordinate comparison needed |
| Win32 | `WM_ACTIVATE`/`WM_KILLFOCUS` on the popup hwnd, or `SetCapture` + detect `WM_LBUTTONDOWN` outside the client rect |
| macOS | local `NSEvent` monitor for `NSEventMaskLeftMouseDown` while shown, or `resignKeyWindow` |

`grab_pointer`/`ungrab_pointer` already exist on `PlatformWindow`
(`window.cpp:252-260`) and are used today for in-window widget capture
(scrollbar dragging, `scrollbar.cpp:238,256`) — X11/Win32 popups would
reuse the same primitive for a different purpose, not need a new one.

### 3.5 `[new]` Toolkit-level integration: `Window::open_popup` grows a second backend

`Menu::show` itself barely changes — it still needs to compute a size and
position and hand off paint/mouse/key callbacks. What changes is what's
underneath `Window::open_popup`: instead of unconditionally pushing a
paint-callback entry onto `popups_`, it would construct a `PopupWindow`
(owning a `PlatformWindow` from §3.1) and forward the platform's native
input callbacks into the same `on_paint`/`on_mouse`/`on_key` functions
`Menu` already builds. The LIFO stacking/closing logic already in
`window.cpp:861-904` (close children before parent, insert a newly-opened
submenu above its parent) still applies conceptually — it just needs to
operate on real windows instead of vector entries.

Positioning moves from window-local (`map_to_window`, clamped to
`window_->size()`, `menu.cpp:81,88-102`) to screen-local, clamped to the
containing monitor's work area — the same math §2.1 already has, just
needs to live somewhere shared (e.g. on the new `PopupWindow`) instead of
being reimplemented by `Menu`.

### 3.6 `[new]` Nested submenus become a real window stack

`Menu::open_submenu` (`menu.cpp:365-395`) positions a child `Popup`
relative to `bounds_` in the same window-local space, and the
hover-elsewhere-closes-the-open-submenu logic
(`Menu::handle_mouse`, `menu.cpp:206-221`) closes it by calling
`window_->close_popup()`. With real windows, each submenu is its own
`PlatformWindow`, visually owned by its parent popup (not modally — just
for stacking/lifetime), and that same "close when hovering a different
item" logic has to close the *native* popup window rather than pop a
vector entry — same trigger, different mechanism underneath.

### 3.7 `[gap]` Always-on-top relative to other applications

A real popup should float above the parent app (and, like a system menu,
above other apps while open). X11 `override_redirect` and Win32's tooltip
styling already imply this by construction; Wayland's `xdg_popup` is
stacked correctly by the compositor per-protocol; macOS needs an explicit
window level (`NSStatusWindowLevel`, already used for the tooltip in
§2.1) rather than the default.

## 4. What does *not* need to change

- `Menu`/`MenuBar`/`ContextMenu`/`Combobox`'s widget-facing API (`show`,
  `handle_mouse`, `handle_key`, `paint`) — item layout, hover math, and
  keyboard nav stay exactly as they are; only *what delivers events to
  them* and *what surface they paint into* changes.
- `Theme::draw_menu_*`/`BaseTheme` — same `Painter` interface either way.
- `grab_pointer`/`ungrab_pointer` — reused for §3.4, not replaced or added.

## 5. Explicitly out of scope here

- Actually implementing any of this — this document is a scoping pass,
  triggered by the simpler text-clipping fix, not a commitment to do the
  larger rewrite.
- Working through `Combobox`/`ContextMenu` item-by-item — both would reuse
  whatever `PopupWindow` primitive comes out of `Menu`'s migration; not
  worth designing three times independently.
- Deciding this is worth doing: the clipped overlay approach already
  covers every case except "popup needs to extend past its owning
  window's edges," which is real but narrow (small windows, multi-monitor
  setups). The bulk of the cost here is the four independent per-platform
  input/dismissal implementations in §3.2/§3.4, which is real, ongoing
  maintenance surface, not a one-time cost. Recommend treating this as a
  backlog item gated on it actually biting someone, not a near-term change.

## 6. Suggested phasing, if pursued

1. `PopupWindow` + Wayland backend first — `xdg_popup`'s grab and
   `popup_done` solve §3.4 for free, making it the cheapest platform to
   validate the toolkit-level integration (§3.5/§3.6) against.
2. X11 second — closest existing code to reuse (override-redirect tooltip
   window + already-existing `grab_pointer`).
3. Win32 and macOS in parallel, once §3.5's `PopupWindow` abstraction is
   proven not to have accidentally baked in X11/Wayland-specific
   assumptions.
4. Migrate `Menu` first (deepest existing test of nesting, §3.6);
   `Combobox`/`ContextMenu` follow once `Menu` is stable on all four
   backends.

## 7. File-level touch points, if pursued

| File | Change |
|---|---|
| `include/toolkit/platform.hpp` | new `PlatformWindow::create_popup` factory (§3.1) |
| `src/toolkit/platform/wayland/wl_platform.cpp` | interactive `xdg_popup` w/ `xdg_popup_grab`, `popup_done` handling |
| `src/toolkit/platform/x11/x11_platform.cpp` | interactive `override_redirect` popup window, input routing, grab-based dismissal |
| `src/toolkit/platform/win32/win32_platform.cpp` | interactive `WS_POPUP` window, window proc, focus-loss dismissal |
| `src/toolkit/platform/macos/macos_native_platform.mm`, `macos_opengl_platform.mm` | interactive borderless `NSWindow`/`NSPanel`, local event monitor |
| `include/toolkit/window.hpp` / `src/toolkit/window.cpp` | `PopupWindow` type; `open_popup`/`close_popup` back it with a real window instead of (or alongside) the current paint-callback overlay |
| `src/toolkit/menu.cpp` | positioning moves to screen-space + monitor clamping instead of `window_->size()` |
| `src/toolkit/context_menu.cpp`, `src/toolkit/combobox.cpp` | same migration once `Menu` validates the approach |
