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

class DirectoryDialog {
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

    explicit DirectoryDialog(Window *parent);

    DirectoryDialog &title(std::string_view t);
    DirectoryDialog &start_path(std::string_view path);
    DirectoryDialog &use_native(bool v = true);

    Future choose();

  private:
    Future show(bool use_native);
    Future show_native();
    Future show_toolkit();

    Window *parent_;
    std::string title_;
    std::string start_path_;
    bool use_native_ = true;
};

} // namespace toolkit
