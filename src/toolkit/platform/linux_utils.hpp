// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"

namespace toolkit {

/**
 * Shared utility functions for Linux-based platforms (X11, Wayland).
 */
namespace linux_utils {

SystemFonts detect_system_fonts();

} // namespace linux_utils
} // namespace toolkit
