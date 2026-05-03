// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace toolkit {

class Window;

class FileDialog {
  public:
    using Result = std::optional<std::string>;
    using Callback = std::function<void(Result)>;

    struct Filter {
        std::string label;   // e.g. "C++ Files"
        std::string pattern; // e.g. "*.cpp *.hpp"
    };

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

    explicit FileDialog(Window *parent);

    FileDialog &title(std::string_view t);
    FileDialog &start_path(std::string_view path);
    FileDialog &default_name(std::string_view name);
    FileDialog &file_must_exist(bool v = true);
    FileDialog &add_filter(std::string_view label, std::string_view pattern);
    FileDialog &use_native(bool v = true);

    Future open();
    Future save();

  private:
    Future show(std::string_view ok_label);
    Future show_native(bool is_save);

    Window *parent_;
    std::string title_;
    std::string start_path_;
    std::string default_name_;
    bool file_must_exist_ = false;
    bool use_native_ = false;
    std::vector<Filter> filters_;
};

} // namespace toolkit
