// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace toolkit {

class Window;

class FileDialog {
  public:
    using Result = std::optional<std::string>;
    using Callback = std::function<void(Result)>;

    class Future {
      public:
        explicit Future(std::shared_ptr<Callback> cb) : callback_(std::move(cb)) {}
        Future &then(Callback cb) {
            *callback_ = std::move(cb);
            return *this;
        }

      private:
        std::shared_ptr<Callback> callback_;
    };

    static Future open(Window *parent, std::string_view title = "Open File",
                       std::string_view start_path = "");

    static Future save(Window *parent, std::string_view title = "Save File",
                       std::string_view start_path = "");
};

} // namespace toolkit
