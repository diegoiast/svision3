// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <string>
#include <string_view>

namespace toolkit {

// UTF-8 <-> UTF-16 conversion for the Win32 wide-char API. Declared here so callers
// (platform, painter, image loader, ...) share one implementation without pulling
// <windows.h> into their headers.
std::wstring utf8_to_wide(std::string_view s);
std::string wide_to_utf8(std::wstring_view w);

} // namespace toolkit
