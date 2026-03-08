// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/events.hpp"
#include <functional>
#include <memory>
#include <string>

namespace toolkit {

struct Shortcut {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool super = false;
    Key key = Key::NoKey;
    char character = 0;

    bool is_empty() const { return key == Key::NoKey && character == 0; }
    bool matches(class KeyEvent const &event) const;
    static Shortcut parse(std::string_view s);
};

class Command {
  public:
    using Ptr = std::shared_ptr<Command>;

    Command(std::string name, std::function<void()> execute,
            std::function<bool()> enabled = nullptr)
        : name_(std::move(name)), execute_(std::move(execute)), enabled_func_(std::move(enabled)) {}

    static Ptr create(std::string name, std::function<void()> execute,
                      std::function<bool()> enabled = nullptr) {
        return std::make_shared<Command>(std::move(name), std::move(execute), std::move(enabled));
    }

    std::string const &name() const { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    std::string const &tooltip() const { return tooltip_; }
    void set_tooltip(std::string tooltip) { tooltip_ = std::move(tooltip); }

    std::string const &shortcut_string() const { return shortcut_string_; }
    Shortcut const &shortcut() const { return shortcut_; }
    void set_shortcut(std::string s);

    bool is_enabled() const { return !enabled_func_ || enabled_func_(); }
    void set_enabled_func(std::function<bool()> func) { enabled_func_ = std::move(func); }

    bool is_checked() const { return checked_func_ && checked_func_(); }
    void set_checked_func(std::function<bool()> func) { checked_func_ = std::move(func); }

    void set_execute_func(std::function<void()> func) { execute_ = std::move(func); }

    void execute() {
        if (is_enabled() && execute_) {
            execute_();
        }
    }

    std::string display_text() const {
        if (name_.empty()) {
            return "???";
        }
        return name_;
    }

    bool matches_key_event(class KeyEvent const &event) const {
        return shortcut_.matches(event);
    }

  private:
    std::string name_;
    std::string tooltip_;
    std::string shortcut_string_;
    Shortcut shortcut_;
    std::function<void()> execute_;
    std::function<bool()> enabled_func_;
    std::function<bool()> checked_func_;
};

} // namespace toolkit
