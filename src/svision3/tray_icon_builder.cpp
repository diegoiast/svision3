// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// The platform-neutral half of TrayIconBuilder: just the setters. build() is
// the one member with a per-platform definition, and lives in whichever
// backend CMake selected -- see include/svision3/tray_icon.hpp.

#include "svision3/tray_icon.hpp"

namespace svision3 {

TrayIconBuilder TrayIcon::builder() { return {}; }

TrayIconBuilder &TrayIconBuilder::icon(Icon icon) {
    icon_ = std::move(icon);
    return *this;
}

TrayIconBuilder &TrayIconBuilder::icon_name(std::string name) {
    icon_name_ = std::move(name);
    return *this;
}

TrayIconBuilder &TrayIconBuilder::tooltip(std::string tooltip) {
    tooltip_ = std::move(tooltip);
    return *this;
}

TrayIconBuilder &TrayIconBuilder::id(std::string id) {
    id_ = std::move(id);
    return *this;
}

TrayIconBuilder &TrayIconBuilder::owner_window(std::shared_ptr<Window> const &window) {
    owner_window_ = window;
    return *this;
}

TrayIconBuilder &TrayIconBuilder::action(Command::Ptr action) {
    actions_.push_back(std::move(action));
    return *this;
}

TrayIconBuilder &TrayIconBuilder::actions(std::vector<Command::Ptr> actions) {
    actions_ = std::move(actions);
    return *this;
}

} // namespace svision3
