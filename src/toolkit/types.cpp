// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/types.hpp"

namespace toolkit {

std::string strip_mnemonic(std::string_view text) {
    auto pos = text.find('&');
    if (pos == std::string_view::npos)
        return std::string{text};
    std::string s{text.substr(0, pos)};
    s += text.substr(pos + 1);
    return s;
}

} // namespace toolkit
