// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Cross-platform system tray icon facade. The class and this header are
// platform-neutral; exactly one of the following gets compiled in per
// target (see CMakeLists.txt) and provides TrayIconBuilder::build()'s
// definition:
//  - src/svision3/linux/tray_icon.cpp -- built when SVISION3_HAS_DBUS is
//    defined (Linux, dbus-1 available). Implemented as a StatusNotifierItem
//    (org.kde.StatusNotifierItem) exported over D-Bus via svision3::dbus.
//    Left-click and right-click do two independent things there:
//     - Right-click: exposed through com.canonical.dbusmenu (ItemIsMenu is
//       false, so the host only auto-opens this on right-click, never on
//       left-click too) -- rendered entirely by the tray host (Plasma's
//       systray, etc.), no toolkit Window/popup involved.
//     - Left-click: the host just calls Activate(), a plain D-Bus method
//       with no host-rendered UI attached to it. Handled as a hide()/show()
//       toggle on the owner window -- the common "click the tray icon to
//       show/hide the app" pattern (Telegram, Slack, Dropbox, ...).
//  - src/svision3/win32/tray_icon.cpp -- Windows, via Shell_NotifyIcon. Same
//    left-click/right-click split, but the menu is ours to draw rather than
//    the host's: TrackPopupMenuEx takes real screen coordinates, so there is
//    no Wayland-style absolute-positioning gap.
//  - src/svision3/tray_icon_stub.cpp -- everywhere else (macOS). build()
//    always returns nullptr, same as the Linux backend does when no D-Bus
//    session bus is reachable, so callers never need to branch on platform.
//    A macOS backend would be NSStatusItem-based, just not implemented yet.
//
// EVENTUALLY (not implemented, revisit if/when actually needed):
//  - A macOS backend (see above).
//  - The icon, tooltip and menu items are all fixed at build() time on every
//    backend. Making them settable afterwards needs LayoutUpdated (DBusMenu)
//    and NewIcon/NewToolTip (SNI) on Linux, and NIM_MODIFY on Windows.
//  - Linux: no re-registration if org.kde.StatusNotifierWatcher restarts,
//    or wasn't running yet at construction time -- see
//    include/svision3/linux/dbus_service.hpp's own EVENTUALLY note on the
//    same gap; this is the same missing piece (subscribe to
//    NameOwnerChanged) applied to this specific watcher name.
//  - Linux: GetGroupProperties (DBusMenu's partial-refresh method) isn't
//    implemented; only GetLayout is, which is what Plasma's systray
//    actually calls for a static flat menu like this one.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <svision3/command.hpp>
#include <svision3/image.hpp>

namespace svision3 {

class Window;
class TrayIconBuilder;

class TrayIcon {
  public:
    // The way to make one -- see TrayIconBuilder below for the setters.
    static TrayIconBuilder builder();

    ~TrayIcon();

    TrayIcon(TrayIcon const &) = delete;
    TrayIcon &operator=(TrayIcon const &) = delete;

  private:
    friend class TrayIconBuilder;
    TrayIcon();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Same shape as ToastBuilder: chainable setters, one terminal build().
//
//   auto tray = TrayIcon::builder()
//                   .icon(app.load_icon("utilities-terminal", 22, ""))
//                   .tooltip("svision3")
//                   .id("svision3-tray")
//                   .owner_window(window)
//                   .actions(std::move(menu_actions))
//                   .build();
//
// build() returns nullptr where there is no tray at all (macOS today, or
// Linux with no reachable D-Bus session bus), so callers check the result
// rather than branching on platform.
class TrayIconBuilder {
  public:
    // The image drawn in the tray. Every backend can use this, so it is the
    // portable way to set the icon; pick a size near the panel's (Windows:
    // GetSystemMetrics(SM_CXSMICON), typically 16; Linux panels vary, 22 is a
    // reasonable default). Falls back to the owner window's icon, and then to
    // a stock application icon, if unset or empty.
    TrayIconBuilder &icon(Icon icon);

    // Optional, and Linux-only in effect: a freedesktop icon-theme name (e.g.
    // "utilities-terminal") published as StatusNotifierItem's IconName. Worth
    // setting in addition to icon() when a name exists, because the tray host
    // then renders it at the panel's own size and re-renders it on icon-theme
    // and DPI changes -- none of which it can do with the fixed bitmap that
    // icon() becomes on the wire (IconPixmap). Ignored everywhere else, and
    // ignored on Linux too if the name doesn't resolve in the user's theme.
    TrayIconBuilder &icon_name(std::string name);

    TrayIconBuilder &tooltip(std::string tooltip);

    // StatusNotifierItem's Id property: a short, stable, application-specific
    // identifier, not shown to the user. Unused on Windows, where
    // Shell_NotifyIcon identifies the icon by (hWnd, uID) instead.
    TrayIconBuilder &id(std::string id);

    // Left-click toggles this window's visibility. Held weakly -- the tray
    // icon never keeps the window alive, and a click after the window is gone
    // is a no-op rather than a dangling deref.
    TrayIconBuilder &owner_window(std::shared_ptr<Window> const &window);

    // Each Command's name() is the menu label, icon_image() (if set) the menu
    // icon, and execute() fires when the item is clicked.
    TrayIconBuilder &action(Command::Ptr action);
    TrayIconBuilder &actions(std::vector<Command::Ptr> actions);

    std::unique_ptr<TrayIcon> build() const;

  private:
    Icon icon_;
    std::string icon_name_;
    std::string tooltip_;
    std::string id_;
    std::weak_ptr<Window> owner_window_;
    std::vector<Command::Ptr> actions_;
};

} // namespace svision3
