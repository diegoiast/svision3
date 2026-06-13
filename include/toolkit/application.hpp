// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/window.hpp"
#include <memory>
#include <string_view>
#include <vector>

namespace toolkit {

class IconProvider;

class Application {
  public:
    Application();
    ~Application();

    static Application &instance();
    static bool has_instance();

    Application(Application const &) = delete;
    Application &operator=(Application const &) = delete;

    Window *create_window(std::string_view title, Size size, WindowOptions options = {});
    int run();
    void run_until(std::function<bool()> should_exit);
    void quit();

    void set_application_name(std::string_view name);
    std::string_view application_name() const;

    std::string_view platform_name() const;

    void notify_theme_changed();

    void set_icon_provider(std::unique_ptr<IconProvider> provider);
    IconProvider *icon_provider() const;
    Icon load_icon(std::string_view icon_name, int size, std::string_view context = "actions");

    static void post_to_main_thread(std::function<void()> fn);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::vector<std::unique_ptr<Window>> windows_;
};

} // namespace toolkit
