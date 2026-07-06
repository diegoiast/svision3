// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/types.hpp"
#include "toolkit/utf8.hpp"
#include <cctype>
#include <format>

namespace toolkit {

std::string normalize_mnemonic_key(std::string_view char_text) {
    auto end = Utf8Iterator::next(char_text, 0);
    auto raw = char_text.substr(0, end);
    if (raw.size() == 1) {
        auto c = static_cast<unsigned char>(raw[0]);
        return std::string(1, static_cast<char>(std::tolower(c)));
    }
    return std::string{raw};
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

} // namespace toolkit
