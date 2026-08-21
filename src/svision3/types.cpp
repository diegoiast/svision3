// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/types.hpp"
#include "svision3/utf8.hpp"
#include <cctype>
#include <format>

namespace svision3 {

// Case folding for the alphabets a mnemonic is realistically written in. A full
// Unicode table would pull in a dependency for no practical gain here, since a
// mnemonic is always a single letter the user can type on their own keyboard.
static auto codepoint_to_lower(uint32_t cp) -> uint32_t {
    if (cp < 0x80) {
        return static_cast<uint32_t>(std::tolower(static_cast<unsigned char>(cp)));
    }
    // Latin-1 Supplement. 0xD7 is the multiplication sign, not a letter.
    if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) {
        return cp + 0x20;
    }
    // Latin Extended-A pairs upper with lower, but the phase shifts twice.
    if (cp >= 0x100 && cp <= 0x137) {
        return (cp % 2 == 0) ? cp + 1 : cp;
    }
    if (cp >= 0x139 && cp <= 0x148) {
        return (cp % 2 == 1) ? cp + 1 : cp;
    }
    if (cp >= 0x14A && cp <= 0x177) {
        return (cp % 2 == 0) ? cp + 1 : cp;
    }
    if (cp == 0x178) {
        return 0xFF;
    }
    if (cp >= 0x179 && cp <= 0x17E) {
        return (cp % 2 == 1) ? cp + 1 : cp;
    }
    if (cp >= 0x391 && cp <= 0x3AB) {
        return cp + 0x20;
    }
    if (cp >= 0x400 && cp <= 0x40F) {
        return cp + 0x50;
    }
    if (cp >= 0x410 && cp <= 0x42F) {
        return cp + 0x20;
    }
    return cp;
}

std::string normalize_mnemonic_key(std::string_view char_text) {
    if (char_text.empty()) {
        return {};
    }
    auto cp = Utf8Iterator::decode(char_text, 0);
    return Utf8Iterator::encode(codepoint_to_lower(cp));
}

MnemonicInfo parse_mnemonic(std::string_view text) {
    auto amp = text.find('&');
    if (amp == std::string_view::npos) {
        return {std::string{text}, {}};
    }
    auto char_start = amp + 1;
    MnemonicInfo r;
    r.text = std::string{text.substr(0, amp)};
    r.text += text.substr(amp + 1);
    if (char_start < text.size()) {
        r.key = normalize_mnemonic_key(text.substr(char_start));
    }
    return r;
}

std::string strip_mnemonic(std::string_view text) { return parse_mnemonic(text).text; }

std::string format_size(std::uintmax_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    if (bytes < 1024 * 1024) {
        return std::to_string(bytes / 1024) + " KB";
    }
    if (bytes < 1024ull * 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }
    return std::to_string(bytes / (1024ull * 1024 * 1024)) + " GB";
}

std::string format_mtime(std::filesystem::file_time_type t) {
    auto sys = std::chrono::clock_cast<std::chrono::system_clock>(t);
    return std::format("{:%Y-%m-%d %H:%M}",
                       std::chrono::zoned_time{std::chrono::current_zone(), sys});
}

} // namespace svision3
