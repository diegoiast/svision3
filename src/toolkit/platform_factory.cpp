#include "toolkit/application.hpp"
#include "toolkit/clipboard.hpp"
#include "toolkit/image_loader.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"

#include <cstdlib>
#include <map>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#if defined(__APPLE__)
#ifdef TOOLKIT_HAS_CAIRO
#include "platform/macos/macos_cairo_platform.hpp"
#endif
#include "platform/macos/macos_native_platform.hpp"
#include "platform/macos/macos_opengl_platform.hpp"
#elif defined(_WIN32)
#include "platform/win32/win32_platform.hpp"
#else
#ifdef TOOLKIT_HAS_X11
#include "platform/x11/x11_platform.hpp"
#endif
#ifdef TOOLKIT_HAS_WAYLAND
#include "platform/wayland/wl_platform.hpp"
#endif
#endif

namespace toolkit {

class DummyIconProvider : public IconProvider {
  public:
    auto load(std::string_view, int, std::string_view) -> Icon override { return nullptr; }
};

static PlatformApplication *s_platform = nullptr;
static Application *s_application = nullptr;

namespace detail {
PlatformApplication *current_platform() { return s_platform; }
void set_current_platform(PlatformApplication *p) { s_platform = p; }
Application *current_application() { return s_application; }
void set_current_application(Application *a) { s_application = a; }
} // namespace detail

std::unique_ptr<PlatformApplication> create_platform_application() {
    const char *env_backend = std::getenv("SVISION_BACKEND");
    if (env_backend && std::string_view(env_backend) == "dummy") {
        return std::make_unique<DummyPlatformApplication>();
    }

#if defined(__APPLE__)
    const char *env = std::getenv("SVISION_PAINT");
    if (env) {
        auto v = std::string_view(env);
#ifdef TOOLKIT_HAS_CAIRO
        if (v == "cairo") {
            return std::make_unique<MacOSCairoPlatformApplication>();
        }
#endif
        if (v == "opengl") {
            return std::make_unique<MacOSOpenGLPlatformApplication>();
        }
    }
    return std::make_unique<MacOSNativePlatformApplication>();
#elif defined(_WIN32)
    return std::make_unique<Win32PlatformApplication>();
#else
    const char *env = env_backend;
    if (env) {
#ifdef TOOLKIT_HAS_WAYLAND
        if (std::string_view(env) == "wayland") {
            try {
                return std::make_unique<WaylandPlatformApplication>();
            } catch (...) {
                spdlog::warn("Wayland init failed");
            }
        }
#endif
#ifdef TOOLKIT_HAS_X11
        if (std::string_view(env) == "x11") {
            return std::make_unique<X11PlatformApplication>();
        }
#endif
    }
#ifdef TOOLKIT_HAS_WAYLAND
    if (std::getenv("WAYLAND_DISPLAY")) {
        try {
            return std::make_unique<WaylandPlatformApplication>();
        } catch (...) {
            spdlog::warn("Wayland init failed, trying X11");
        }
    }
#endif
#ifdef TOOLKIT_HAS_X11
    if (std::getenv("DISPLAY")) {
        return std::make_unique<X11PlatformApplication>();
    }
#endif
    spdlog::warn("No suitable display backend found, falling back to dummy");
    return std::make_unique<DummyPlatformApplication>();
#endif
}

struct Application::Impl {
    std::unique_ptr<PlatformApplication> platform;
    std::unique_ptr<IconProvider> icon_provider;
    std::map<std::string, Icon> icon_cache;
};

Application::Application() : impl_(std::make_unique<Impl>()) {
    detail::set_current_application(this);
    impl_->platform = create_platform_application();
    detail::set_current_platform(impl_->platform.get());

    impl_->icon_provider = std::make_unique<DummyIconProvider>();

    // Refresh theme now that platform is active to detect correct fonts/scale
    Theme::set_current(Theme::create(Theme::detect_system_style()));

    auto const &theme = Theme::current();
    spdlog::info("Theme: {} (Font: '{}' {}px, Monospace: '{}', Scale: {:.2f})", theme.name,
                 theme.system_font, theme.palette.font_size, theme.monospace_font,
                 impl_->platform->scale_factor());
}

Application::~Application() {
    detail::set_current_platform(nullptr);
    detail::set_current_application(nullptr);
}

Application &Application::instance() { return *detail::current_application(); }

Window *Application::create_window(std::string_view title, Size size) {
    windows_.push_back(std::make_unique<Window>(title, size));
    return windows_.back().get();
}

int Application::run() { return impl_->platform->run(); }
void Application::quit() { impl_->platform->quit(); }

std::string_view Application::platform_name() const { return impl_->platform->name(); }

void Application::notify_theme_changed() {
    for (auto &win : windows_) {
        win->on_theme_changed();
    }
}

void Application::set_icon_provider(std::unique_ptr<IconProvider> provider) {
    if (provider) {
        impl_->icon_provider = std::move(provider);
    } else {
        impl_->icon_provider = std::make_unique<DummyIconProvider>();
    }
    impl_->icon_cache.clear();
}

IconProvider *Application::icon_provider() const { return impl_->icon_provider.get(); }

Icon Application::load_icon(std::string_view icon_name, int size, std::string_view context) {
    auto key = fmt::format("{}:{}:{}", icon_name, size, context);
    auto it = impl_->icon_cache.find(key);
    if (it != impl_->icon_cache.end()) {
        return it->second;
    }

    auto icon = impl_->icon_provider->load(icon_name, size, context);
    if (icon) {
        impl_->icon_cache[key] = icon;
    }
    return icon;
}

void Application::post_to_main_thread(std::function<void()> fn) {
    if (s_platform) {
        s_platform->post_to_main_thread(std::move(fn));
    }
}

std::string Clipboard::get_text() {
    if (!s_platform) {
        return {};
    }
    return s_platform->clipboard_get_text();
}

void Clipboard::set_text(std::string const &text) {
    if (s_platform) {
        s_platform->clipboard_set_text(text);
    }
}

} // namespace toolkit
