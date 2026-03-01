// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <functional>
#include <string>

namespace toolkit {

class Command {
  public:
    Command(std::string name, std::function<void()> execute,
            std::function<bool()> enabled = nullptr)
        : name_(std::move(name)), execute_(std::move(execute)), enabled_(std::move(enabled)) {}

    std::string const &name() const { return name_; }
    bool is_enabled() const { return !enabled_ || enabled_(); }
    void execute() {
        if (is_enabled() && execute_) {
            execute_();
        }
    }

  private:
    std::string name_;
    std::function<void()> execute_;
    std::function<bool()> enabled_;
};

} // namespace toolkit
