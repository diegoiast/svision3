// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// No-op fallback for platforms without a real tray-icon backend yet
// (currently: macOS only -- see include/toolkit/tray_icon.hpp for the real
// implementations, src/toolkit/linux/tray_icon.cpp and
// src/toolkit/win32/tray_icon.cpp). build() always returns nullptr, exactly
// like the Linux backend does when no D-Bus session bus is reachable, so
// callers never need to branch on platform.

#include "toolkit/tray_icon.hpp"
#include <spdlog/spdlog.h>

namespace toolkit {

struct TrayIcon::Impl {};

TrayIcon::TrayIcon() : impl_(std::make_unique<Impl>()) {}
TrayIcon::~TrayIcon() = default;

std::unique_ptr<TrayIcon> TrayIconBuilder::build() const {
    spdlog::warn("TrayIcon: not implemented on this platform yet");
    return nullptr;
}

} // namespace toolkit
