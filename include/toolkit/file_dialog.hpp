// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <future>
#include <optional>
#include <string>
#include <string_view>

namespace toolkit {

class Window;

class FileDialog {
  public:
    using Result = std::future<std::optional<std::string>>;

    static Result open(Window *parent, std::string_view title = "Open File",
                       std::string_view start_path = "");

    static Result save(Window *parent, std::string_view title = "Save File",
                       std::string_view start_path = "");
};

} // namespace toolkit
