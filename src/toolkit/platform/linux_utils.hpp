// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"
#include <string>

namespace toolkit {

/**
 * Shared utility functions for Linux-based platforms (X11, Wayland).
 */
namespace linux_utils {

SystemFonts detect_system_fonts();

// Best-effort name of the desktop's configured XDG icon theme, checked in
// this order: GTK's settings.ini (gtk-icon-theme-name, honored by GNOME,
// XFCE, Cinnamon, MATE, ...), then KDE's kdeglobals ([Icons] Theme=). Returns
// an empty string if neither config is found.
std::string detect_system_icon_theme();

// Call after FcInit(). Loads the system fontconfig alias rules (conf.d) that
// the conan static fontconfig misses because its baked-in prefix is wrong.
void init_fontconfig();

} // namespace linux_utils
} // namespace toolkit
