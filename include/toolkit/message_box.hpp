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
    using Callback = std::function<void(MessageBoxResult)>;

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

    // Builder API
    explicit MessageBox(Window *parent);

    MessageBox &title(std::string_view t);
    MessageBox &message(std::string_view m);
    MessageBox &markdown(bool enabled = true);
    MessageBox &icon(MessageBoxIcon i);
    MessageBox &buttons(MessageBoxButtons b);

    Future show();

    // Static convenience methods
    static std::future<MessageBoxResult>
    show_static(Window *parent, std::string_view title, std::string_view message,
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

  private:
    Window *parent_;
    std::string title_;
    std::string message_;
    MessageBoxIcon icon_ = MessageBoxIcon::Information;
    MessageBoxButtons buttons_ = MessageBoxButtons::Ok;
    bool markdown_ = false;
};

} // namespace toolkit
