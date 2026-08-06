// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Cross-platform system tray icon facade. The class and this header are
// platform-neutral; exactly one of the following gets compiled in per
// target (see CMakeLists.txt) and provides TrayIcon::create()'s definition:
//  - src/toolkit/linux/tray_icon.cpp -- the only real backend today. Built
//    when TOOLKIT_HAS_DBUS is defined (Linux, dbus-1 available). Implemented
//    as a StatusNotifierItem (org.kde.StatusNotifierItem) exported over
//    D-Bus via toolkit::dbus. Left-click and right-click do two independent
//    things there:
//     - Right-click: exposed through com.canonical.dbusmenu (ItemIsMenu is
//       false, so the host only auto-opens this on right-click, never on
//       left-click too) -- rendered entirely by the tray host (Plasma's
//       systray, etc.), no toolkit Window/popup involved.
//     - Left-click: the host just calls Activate(), a plain D-Bus method
//       with no host-rendered UI attached to it. Handled as a hide()/show()
//       toggle on `owner_window` -- the common "click the tray icon to
//       show/hide the app" pattern (Telegram, Slack, Dropbox, ...).
//  - src/toolkit/tray_icon_stub.cpp -- everywhere else (macOS, Windows).
//    create() always returns nullptr, same as the Linux backend does when
//    no D-Bus session bus is reachable, so callers never need to branch on
//    platform. A macOS backend would be NSStatusItem-based; Windows would be
//    Shell_NotifyIcon-based -- both are more capable than SNI/DBusMenu here
//    (e.g. TrackPopupMenu takes real screen coordinates, so there's no
//    Wayland-style absolute-positioning gap), just not implemented yet.
//
// EVENTUALLY (not implemented, revisit if/when actually needed):
//  - macOS and Windows backends (see above).
//  - Linux: the right-click menu is fixed at construction. DBusMenu's
//    LayoutUpdated signal (which would let it change afterwards) isn't
//    wired up.
//  - Linux: no re-registration if org.kde.StatusNotifierWatcher restarts,
//    or wasn't running yet at construction time -- see
//    include/toolkit/linux/dbus_service.hpp's own EVENTUALLY note on the
//    same gap; this is the same missing piece (subscribe to
//    NameOwnerChanged) applied to this specific watcher name.
//  - Linux: IconName only (a freedesktop icon-theme name) -- no IconPixmap
//    (raw ARGB data) support, so there's no way to show an icon that isn't
//    already in the user's icon theme.
//  - Linux: GetGroupProperties (DBusMenu's partial-refresh method) isn't
//    implemented; only GetLayout is, which is what Plasma's systray
//    actually calls for a static flat menu like this one.

#pragma once

#include "toolkit/command.hpp"
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class Window;

class TrayIcon {
  public:
    // icon_name: a freedesktop icon-theme name (e.g. "utilities-terminal").
    // id: StatusNotifierItem's Id property -- a short, stable,
    // application-specific identifier (not shown to the user).
    // owner_window: must outlive the TrayIcon; left-click toggles its
    // visibility (see class comment above).
    // right_click_actions: each Command's name() is the menu label, icon()
    // (if set) the menu icon, and execute() fires when the item is clicked.
    // Returns nullptr if no D-Bus session bus is reachable.
    static std::unique_ptr<TrayIcon> create(std::string icon_name, std::string tooltip,
                                            std::string id, Window *owner_window,
                                            std::vector<Command::Ptr> right_click_actions);
    ~TrayIcon();

    TrayIcon(TrayIcon const &) = delete;
    TrayIcon &operator=(TrayIcon const &) = delete;

  private:
    TrayIcon();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace toolkit
