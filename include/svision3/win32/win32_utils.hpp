// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <string>
#include <string_view>

namespace svision3 {

// UTF-8 -> UTF-16, for handing toolkit strings to a wide Win32 API (CreateWindowW,
// the *W file APIs, ...).
//
// Counted, not NUL-terminated: exactly s.size() bytes are converted, so embedded
// NULs survive and the result carries no terminator of its own. Empty in, empty out.
//
// Byte sequences that are not valid UTF-8 are replaced with U+FFFD rather than
// rejected, matching MultiByteToWideChar's default behaviour (MB_ERR_INVALID_CHARS
// is deliberately not set). The result is UTF-16, so characters outside the BMP
// come back as surrogate pairs and contribute two wchar_t each -- size() counts
// UTF-16 code units, not codepoints.
std::wstring utf8_to_wide(std::string_view s);

// UTF-16 -> UTF-8, for bringing a wide Win32 result back into toolkit strings
// (window titles, file paths, account names, ...).
//
// Counted, not NUL-terminated: exactly w.size() code units are converted. Passing a
// NUL-terminated Win32 buffer therefore means building the view first --
// std::wstring_view{ptr} measures with wcslen and so excludes the terminator -- while
// a fixed-size buffer that may not be terminated needs its length given explicitly.
// Empty in, empty out.
//
// Unpaired surrogates -- which a Win32 buffer can legitimately contain, since
// Windows does not enforce well-formed UTF-16 -- are replaced with U+FFFD rather
// than rejected: WC_ERR_INVALID_CHARS is deliberately not set, and setting it would
// instead fail the whole call with ERROR_NO_UNICODE_TRANSLATION.
std::string wide_to_utf8(std::wstring_view w);

} // namespace svision3
