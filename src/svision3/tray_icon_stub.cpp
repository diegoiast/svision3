// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// No-op fallback for platforms without a real tray-icon backend yet
// (currently: macOS only -- see include/svision3/tray_icon.hpp for the real
// implementations, src/svision3/linux/tray_icon.cpp and
// src/svision3/win32/tray_icon.cpp). build() always returns nullptr, exactly
// like the Linux backend does when no D-Bus session bus is reachable, so
// callers never need to branch on platform.

#include "svision3/tray_icon.hpp"
#include <spdlog/spdlog.h>

namespace svision3 {

struct TrayIcon::Impl {};

TrayIcon::TrayIcon() : impl_(std::make_unique<Impl>()) {}
TrayIcon::~TrayIcon() = default;

std::unique_ptr<TrayIcon> TrayIconBuilder::build() const {
    spdlog::warn("TrayIcon: not implemented on this platform yet");
    return nullptr;
}

} // namespace svision3
