// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <string>

namespace toolkit {

class Clipboard {
  public:
    static std::string get_text();
    static void set_text(std::string const &text);
};

} // namespace toolkit
