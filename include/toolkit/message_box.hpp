// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <future>
#include <string_view>

namespace toolkit {

class Window;

enum class MessageBoxIcon { None, Information, Warning, Error, Question };
enum class MessageBoxButtons { Ok, OkCancel, YesNo, YesNoCancel };
enum class MessageBoxResult { Ok, Cancel, Yes, No };

class MessageBox {
  public:
    static std::future<MessageBoxResult> show(Window *parent, std::string_view title,
                                              std::string_view message,
                                              MessageBoxIcon icon = MessageBoxIcon::Information,
                                              MessageBoxButtons buttons = MessageBoxButtons::Ok);

    static std::future<MessageBoxResult> information(Window *parent, std::string_view title,
                                                     std::string_view message);

    static std::future<MessageBoxResult> warning(Window *parent, std::string_view title,
                                                 std::string_view message);

    static std::future<MessageBoxResult> error(Window *parent, std::string_view title,
                                               std::string_view message);

    static std::future<MessageBoxResult> question(Window *parent, std::string_view title,
                                                  std::string_view message);
};

} // namespace toolkit
