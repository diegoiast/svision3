// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace toolkit {

class Window;

class FileDialog {
  public:
    static std::optional<std::string> open(Window *parent,
                                           std::string_view title = "Open File",
                                           std::string_view start_path = "");

    static std::optional<std::string> save(Window *parent,
                                           std::string_view title = "Save File",
                                           std::string_view start_path = "");
};

} // namespace toolkit
