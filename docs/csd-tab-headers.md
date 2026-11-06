# CSD Tab Headers

Scoping and implementation plan for putting reusable tab headers into a window's client-side
decorations (CSD), replacing the title label, while keeping native decorations and existing
`TabWidget` behavior predictable.

Status markers: `[exists]` already in the tree and reusable as-is, `[gap]` exists but needs
extension, `[new]` not implemented.

## 1. Goal

Applications should be able to build browser-like or dock-like tabs in the title bar of a CSD
window:

```text
[icon] [leading CSD widget?] [tab headers ............. trailing text?] [trailing CSD widget?] [buttons]
```

The tab strip replaces the normal title label. The window title still exists for the OS taskbar,
Alt-Tab, accessibility and `Window::title()`, but it is not painted in the CSD title bar while a
center widget is installed.

## 2. Non-goals

- No automatic `Window::set_title()` → tab-bar text binding. When the app changes the title, the app
  must explicitly update the tab bar trailing text (or not).
- No tab strip under native decorations. `Window::set_csd_center_widget()` rejects native mode with
  `false` and a warning log.
- No registration of `TabBar` in `WidgetLoader` in v1.
- No change to `TabWidget`'s existing public API or JSON shape; new opt-in `detach_tab_bar()` /
  `attach_tab_bar()` methods are the only exception.
- No display of window title alongside the center widget; installing a center widget removes the
  title label from the layout entirely.
- No link activation or other mouse interaction in the trailing text in v1.
- No separate public `TabContainer` widget; `TabWidget` itself is the before/after object that owns
  the stack and can detach its bar.

## 3. User decisions already made

- Reusable `TabBar`, not a one-off for one `TabWidget`.
- Generic CSD center-slot API, not a TabBar-only API.
- The center widget replaces the title label.
- Trailing text goes in the empty area before the existing trailing widget; plain and markdown APIs.
- Trailing text default alignment is `Alignment::Start`, adjustable through a separate setter.
- Native mode: do not install the widget, return `false`, log a warning.
- No automatic title synchronization; the application does it explicitly.
- No `TabContainer` indirection. `TabWidget` owns the `TabBar`; for CSD it can detach the same bar
  and leave itself as the content stack.

## 4. Proposed public API

### 4.1 `Window::set_csd_center_widget` `[new]`

```cpp
// Window
bool set_csd_center_widget(std::shared_ptr<Widget> widget);
```

Semantics:

- If the effective decoration mode is `Native`:
  - leave the title bar unchanged;
  - return `false`;
  - `spdlog::warn` explaining that CSD is required and suggesting the app call
    `app.set_decoration_mode(DecorationMode::ThemeCsd | PlatformCsd | PlatformCsdWithAppPalette)` or
    pass `WindowOptions::decoration`.
- If the effective decoration mode is CSD (`ThemeCsd`, `PlatformCsd`, `PlatformCsdWithAppPalette`):
  - store `widget`;
  - replace the title label with it;
  - return `true`.
- `nullptr` clears the center widget and restores the title label.
- The window keeps the widget across CSD rebuilds, theme changes, and decoration-mode changes.
- The widget is inserted with stretch `1` and `Alignment::Fill`; it fills the same area the title
  label would have filled, without increasing the title-bar height.

The position of the center widget is the old title-label slot:

```text
[icon] [leading] [center widget] [trailing] [window buttons]
```

`CsdWidgetPosition::Leading` still inserts before the center slot and
`CsdWidgetPosition::Trailing` still inserts after it.

### 4.2 `TabBar` `[new]`

New public widget extracted from `TabWidget`:

```cpp
class TabBar : public Widget, public Fluent<TabBar> {
    DECLARE_WIDGET(TabBar)
  public:
    TabBar();

    // Tab model / state.
    int add_tab(std::string title, bool closable = true);
    void remove_tab(int index);
    TabBar &set_current(int index);
    int get_current_index() const;
    size_t get_tab_count() const;
    std::string get_tab_title(int index) const;
    TabBar &set_tab_tooltip(int index, std::string tooltip);

    TabBar &set_tabs_closable(bool closable);
    TabBar &set_min_tab_width(float width);
    TabBar &set_tab_font_size(float size);
    TabBar &set_tabs_movable(bool movable);
    TabBar &set_orientation(TabOrientation orientation);

    std::weak_ptr<Widget> set_leading_widget(std::shared_ptr<Widget> widget);
    std::weak_ptr<Widget> set_trailing_widget(std::shared_ptr<Widget> widget);

    [[nodiscard]] Subscription on_current_changed(std::function<void(int index)> callback);
    std::function<void(int index, std::string const &title)> on_tab_close;

    // Optional display-only text in the empty area before trailing_widget_.
    TabBar &set_trailing_text(std::string text);       // plain, escaped
    TabBar &set_trailing_markdown(std::string markdown); // parsed markdown
    TabBar &clear_trailing_text();
    TabBar &set_trailing_text_alignment(Alignment alignment); // default Alignment::Start

    void set_theme_provider(std::function<Theme const &()> provider); // defaults to Theme::current()
};
```

`TabBar` owns no content stack. It is the tab header strip only.

### 4.3 `TabWidget` detach/attach `[new]`

No separate `TabContainer` widget. `TabWidget` is rewritten as a linear layout containing the same
`TabBar` and a `StackedLayout`; detaching leaves the stack behind and gives the app the same bar
for the CSD title bar:

```cpp
class TabWidget : public Widget, public Fluent<TabWidget> {
  public:
    // The default embedded bar is removed from the widget and returned. The widget becomes
    // stack-only. The returned pointer is shared; TabWidget keeps access for synchronization.
    std::shared_ptr<TabBar> detach_tab_bar();

    // Re-embed the same bar above the stack.
    TabWidget &attach_tab_bar();
};
```

Ownership and lifetime:

- `detach_tab_bar()` does not destroy anything and does not transfer sole ownership. It flips
  `TabWidget` to stack-only mode and returns the shared `TabBar`.
- `TabWidget` keeps the bar alive through its shared pointer even if the window rebuilds chrome.
- `Window::set_csd_center_widget(...)` may also hold the same shared `TabBar`; rebuilds reinsert it.
- `attach_tab_bar()` re-inserts the same bar; no duplicate tab model is created.

CSD use:

```cpp
auto tabs = std::make_shared<TabWidget>();
tabs->add_tab("One", make_page_one());
tabs->add_tab("Two", make_page_two());
auto bar = tabs->detach_tab_bar();
if (!window->set_csd_center_widget(bar)) {
    tabs->attach_tab_bar(); // native or no CSD: keep normal embedded tabs
}
window->set_root(tabs);
bar->set_trailing_text("Untitled — modified"); // explicit, app-owned sync
```

Restoring embedded tabs later:

```cpp
window->set_csd_center_widget(nullptr);
tabs->attach_tab_bar();
```

Normal use is unchanged:

```cpp
auto tabs = std::make_shared<TabWidget>();
tabs->add_tab("One", page1);
window->set_root(tabs); // embedded tab bar above content, as before
```

Why not `set_tabbar(std::shared_ptr<TabBar>)`:

- accepting an arbitrary foreign bar complicates ownership, add/remove ordering and re-attach
  semantics;
- same-object detach/attach reuses the existing synchronization wiring;
- the app can still hold the bar pointer after detaching for trailing text and close handlers.

## 5. Refactoring plan

### 5.1 Extract `TabBar` from `TabWidget` `[gap]`

`TabWidget` today mixes the tab header strip and the content stack. Split it:

Move to `TabBar`:

- tab model (`tabs_`), current index, subscriptions
- measuring/layout of tabs, scroll offset/buttons, overflow
- hit-testing, hover, drag/reorder, close buttons
- leading/trailing widgets inside the bar
- bar paint code and the separator line for the bar's orientation
- `tabs_closable`, `min_tab_width`, `tab_font_size`, `tabs_movable`, orientation APIs
- theme access through a provider seam instead of hard-calling `Theme::current()`
- trailing-text APIs

Keep in `TabWidget`:

- `StackedLayout content_layout_`
- embedded `std::shared_ptr<TabBar> tab_bar_`
- focus-on-tab-change behavior (`focus_content_on_change`, `focus_on_tab_click`)
- collapsible/dock behavior
- content halves of paint/mouse/key/focus traversal
- public `TabWidget` API by forwarding to `TabBar` and/or `StackedLayout`

`TabWidget` becomes, internally, a linear container for `TabBar + StackedLayout` (or `StackedLayout`
alone after detach) rather than a fully custom-painted/layout widget. Orientation keeps current
semantics for embedded/docked tabs; CSD detached mode is effectively North/header-in-titlebar.

`TabWidget` JSON must remain byte-compatible in key set and structure. `repro_tab_orientation.cpp`
and existing serialization tests must pass unchanged.

### 5.2 Detach/attach implementation `[new]`

`TabWidget` stores:

```cpp
std::shared_ptr<TabBar> tab_bar_;
std::unique_ptr<StackedLayout> content_layout_;
bool tab_bar_embedded_ = true;
```

- `detach_tab_bar()`:
  - removes `tab_bar_` from the layout and switches to stack-only layout/paint;
  - returns `tab_bar_`;
  - leaves subscriptions and page synchronization intact.
- `attach_tab_bar()`:
  - re-inserts `tab_bar_` above/alongside the stack according to orientation;
  - switches back to embedded mode.
- `size_hint()`, `paint()`, `handle_mouse()`, `set_window()`, `on_theme_changed()` and
  `for_each_child()` branch on `tab_bar_embedded_`; content-only mode must not paint the bar or
  reserve space for it.

### 5.3 `WindowTitleBar` center slot `[gap]`

Add a generic center slot to `WindowTitleBar`:

- default center widget is the existing `title_label`;
- `set_center_widget(widget)` removes the title label and inserts `widget` at the same index;
- `clear_center_widget()` removes `widget` and re-inserts the title label;
- `center_anchor_index()` returns the active center item so `set_csd_widget(... Leading/Trailing)`
  keeps inserting on the correct sides.

All six theme title bars keep creating the default title label. The replacement is centralized in
`WindowTitleBar`, so per-theme changes stay minimal.

`paint()` skips title text painting when a center widget is installed. `widget_at()`, mnemonics and
focus traversal include the center widget. Hit-testing on empty areas of a `TabBar` returns no child,
so the title bar can fall back to window drag / double-click maximize / system menu.

### 5.4 `Window` ownership and rebuilds `[gap]`

`Window` stores `csd_center_widget_` separately from `leading_csd_widget_` / `trailing_csd_widget_`.

`rebuild_chrome()` creates the title bar from the decoration theme, then reapplies:

- center widget (`apply_csd_center_widget()`),
- leading widget,
- trailing widget.

The center widget is not released by the layout when the title bar is destroyed; it remains owned by
the window and/or `TabWidget` and is re-inserted after rebuild.

`Window::on_theme_changed()` and `Window::set_decoration_mode()` therefore preserve the strip:
same model, same active tab, new chrome.

## 6. Decoration-mode behavior matrix

| Mode | Icon/title bar | Center widget installed? | Palette used by strip |
|---|---|---|---|
| `Native` | OS/WM | No — `set_csd_center_widget` returns `false` and logs | n/a |
| `PlatformCsd` | CSD, platform layout/glyphs/palette | Yes | Platform palette |
| `PlatformCsdWithAppPalette` | CSD, platform layout/glyphs, app palette | Yes | App palette (`Theme::current()`) |
| `ThemeCsd` | CSD, app theme | Yes | App palette |
| `Native` + `needs_csd()` (e.g. GNOME Wayland) | Degrades to `PlatformCsd` | Yes | Platform palette |

There is no fallback strip under native decorations. The app pattern is: detach, try to install,
reattach on failure.

## 7. Trailing text in the empty tab-bar area `[new]`

### 7.1 Layout

```text
[leading widget] [tabs / scroll] [empty space / trailing text] [trailing widget]
```

- Optional; if unset/cleared, layout is unchanged.
- If `trailing_widget_` is set, trailing text is placed before it.
- If `trailing_widget_` is not set, trailing text sits in the empty area before the window buttons.
- Clamped to the available empty width; one line; ellipsized/clipped; never increases title-bar height.
- `set_trailing_text_alignment(Alignment::Start|Center|End)` controls alignment inside the empty
  area; default `Alignment::Start`.

### 7.2 Appearance and input

Plain text uses a one-line label. Markdown uses a `RichLabel`-style widget.

In v1 both are display-only:

- not focusable;
- `blocks_hit_test() == false`, so empty text area still falls back to window dragging;
- no link activation;
- does not consume double-click maximize or system-menu gestures.

### 7.3 Title synchronization is app-owned

`Window::set_title()` does not touch the tab bar in v1. If the app wants the strip to mirror the
title, it does so explicitly:

```cpp
window->set_title("Untitled — modified");
bar->set_trailing_markdown("**Untitled** — modified");
```

Rationale: the window should not guess whether app-provided text is meant to follow its title. A
later opt-in binding can be added without changing this API.

### 7.4 Serialization

Trailing text is not serialized in v1. `TabWidget` JSON remains unchanged and does not gain
`trailing_text`. If `TabBar` is ever registered in `WidgetLoader`, add `trailing_text`,
`trailing_text_alignment` and the plain/markdown mode together.

## 8. Theming and PlatformCsd

`TabBar` accesses theme through an injectable provider instead of always calling
`Theme::current()`:

```cpp
bar->set_theme_provider([this] { return window_->decoration_theme(); });
```

- CSD host (`WindowTitleBar`) injects `decoration_theme()`.
- Plain/embedded `TabWidget` keeps the default provider (`Theme::current()`).
- `PlatformCsd`: geometry and palette from the detected host theme.
- `PlatformCsdWithAppPalette`: geometry/glyphs from the host theme; palette from the app theme.
- `ThemeCsd`: fully app-themed.

`draw_tab`, `draw_tab_bar_background`, `measure_tab`, and `style.tabWidget.*` already exist on all
`BaseTheme`-derived themes, so no per-platform tab drawing work is expected.

## 9. Focus, keyboard and mouse rules

- The center tab strip is chrome, not a command context and not a focus stop for document cycling.
- Tabs are clickable; close controls work; scroll buttons work when overflow happens.
- Empty strip area behaves like the title bar: drag to move, double-click to maximize/restore,
  right-click for system menu where supported.
- Presses on a tab must not start a window drag or maximize on double-click.
- `Ctrl+PageUp` / `Ctrl+PageDown` switch tabs. In CSD placement these are reached via the
  non-focus-input path after global commands; no current command binds these chords. If a clash is
  found, move strip key handling ahead of global commands.
- Alt-mnemonics inside leading/trailing widgets (e.g. a CSD `&Menu` button) keep working.

## 10. Tests

Add `tests/test_csd_center_widget.cpp` (or extend decoration tests):

- native mode: `set_csd_center_widget` returns `false`, title label remains, no crash; if the app
  detached first, `attach_tab_bar()` restores embedded tabs;
- `ThemeCsd`, `PlatformCsd`, `PlatformCsdWithAppPalette`: returns `true`, title label removed,
  center widget measured into the title bar at the same height;
- `nullptr` restores the title label; `attach_tab_bar()` restores embedded tabs;
- `detach_tab_bar()` leaves `TabWidget` stack-only: no bar painted, no bar height reserved,
  content fills the widget;
- `attach_tab_bar()` re-embeds the same bar; active index and tabs are unchanged;
- leading/center/trailing order is correct with both leading and trailing CSD widgets set;
- replacing center widget with a different widget swaps it atomically;
- theme change and decoration-mode change preserve the strip and active tab;
- pressing a tab does not start a system move and does not double-click maximize;
- empty strip area does start a system move / double-click maximize;
- trailing text: plain set/clear, markdown set/clear, alignment `Start`/`Center`/`End`, clamps to
  available width, is not focusable, does not block window dragging;
- `Window::title()` still returns the new title; title text is not painted while a center widget is
  installed;
- application-driven title sync works: `set_title()` then explicit `set_trailing_text(...)`;
- `TabWidget` serialization remains byte-compatible (`repro_tab_orientation` and existing tests).

Keep passing: `[tab]`, `[decoration]`, `[window][titlebar]`, dock/plugin/command-anchor suites.

## 11. Backward compatibility

- Existing windows are unchanged; `set_csd_center_widget` is opt-in.
- Existing `TabWidget` public API is unchanged except two new opt-in methods
  (`detach_tab_bar()`, `attach_tab_bar()`).
- Existing `TabWidget` JSON is unchanged.
- Existing CSD widget API (`set_csd_widget`) is unchanged and works around the center widget.
- `PlatformCsd` behavior (host palette fixed across app theme changes) is preserved;
  `PlatformCsdWithAppPalette` is the opt-in app-palette variant.

## 12. Suggested implementation order

1. Extract `TabBar`; rewrite `TabWidget` as embedded `TabBar + StackedLayout`; keep `[tab]` tests green.
2. Add `TabWidget::detach_tab_bar()` / `attach_tab_bar()` and stack-only mode.
3. Add `WindowTitleBar` center slot support (default title label remains interchangeable).
4. Add `Window::set_csd_center_widget` with native `false` + warning.
5. Wire theme-provider injection from the title bar to the strip.
6. Add trailing-text APIs and layout to `TabBar`.
7. Add tests.
8. Write/update docs as the implementation lands.
9. Verify debug build and focused test suites.

## 13. Open or deferred items

- Native fallback UI: out of scope beyond explicit `attach_tab_bar()`. Apps needing tabs under
  native decorations can reattach embedded tabs or use ordinary `TabWidget` in the content area.
- Automatic title → tab text binding: deferred; app-owned for now.
- Link activation/interactivity for markdown trailing text: deferred.
- `WidgetLoader` registration for `TabBar`: deferred.
- Middle-click-to-close tabs: deferred; current close is left-click.
- Richer error reporting than `bool` + warning: deferred; add an
  `enum class CsdCenterWidgetResult` later if callers need programmatic handling.
- Collapsible tab strips in CSD: collapse semantics stay content-side; CSD tabs are not collapsible.
- Very small window widths: overflow/scroll behavior needs manual verification; if the strip is
  starved, prefer eliding titles over dropping close buttons.
