// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/window.hpp"
#include <cassert>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace svision3 {

class IconProvider;

class Application {
  public:
    // Reads the SVISION_LOG_LEVEL environment variable (trace, debug, info, warning, error,
    // critical, off, or their numeric 0-6 equivalents), same convention as SVISION_BACKEND /
    // SVISION_PAINT, and applies it via set_log_level().
    Application();
    ~Application();

    static Application &instance();
    static bool has_instance();

    Application(Application const &) = delete;
    Application &operator=(Application const &) = delete;

    // Parses a spdlog level name (trace, debug, info, warning, error, critical, off) or its
    // numeric 0-6 equivalent, and applies it via spdlog::set_level(). Returns false (and logs a
    // warning) if the value is unrecognized.
    static bool set_log_level(std::string_view name);

    // Application-wide default decoration mode for windows whose WindowOptions request Inherit.
    // A per-window `.decoration` (or the legacy `.csd = true`) still wins. A platform that lacks
    // server-side decorations entirely (PlatformApplication::needs_csd(), e.g. GNOME/mutter's
    // Wayland compositor) can still force CSD over a Native request. Native by default. Applies
    // to windows created after the call. Concrete modes are Native, PlatformCsd,
    // PlatformCsdWithAppPalette and ThemeCsd.
    //
    // `mode` must be concrete: the application is the root of the decoration inheritance chain,
    // so Inherit has nothing to inherit from here. This is a precondition, and a C++ contract
    // would be the right way to express it, but contracts are not available yet. Until they are,
    // enforce it with a runtime assert instead.
    void set_decoration_mode(DecorationMode mode) {
        assert(mode != DecorationMode::Inherit &&
               "Application::set_decoration_mode: Inherit is not a concrete mode");
        decoration_mode_ = mode;
    }
    DecorationMode decoration_mode() const { return decoration_mode_; }

    // The Application keeps a reference for the process lifetime, so the
    // returned pointer stays valid without the caller holding on to it -- take
    // it by `auto` and use it like the raw pointer this used to return.
    //
    // Anything that outlives the call and refers back to the window (a widget
    // callback, a tray icon, a timer) must store std::weak_ptr<Window> and
    // lock() at use. Storing the shared_ptr instead keeps the window alive
    // past close(), and storing it in something the window itself owns (a
    // widget, a callback on one) is a reference cycle that never frees.
    std::shared_ptr<Window> create_window(std::string_view title, Size size,
                                          WindowOptions options = {});
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

    // Tries to configure the icon provider from the desktop's configured XDG
    // icon theme (read from GTK's gtk-icon-theme-name setting, falling back
    // to a desktop-environment heuristic from XDG_CURRENT_DESKTOP). Returns
    // true and installs it if one was found and loaded successfully; returns
    // false (and installs nothing) otherwise, leaving the caller to decide
    // what to fall back to, e.g.:
    //   if (!app.use_xdg_icons()) {
    //       app.set_icon_provider(std::make_unique<XdgImageLoader>("hicolor"));
    //   }
    // To always use a specific theme regardless of the system (no detection
    // at all), skip this and call set_icon_provider() directly instead.
    bool use_xdg_icons();

    static void post_to_main_thread(std::function<void()> fn);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    DecorationMode decoration_mode_ = DecorationMode::Native;
    std::vector<std::shared_ptr<Window>> windows_;
};

} // namespace svision3
