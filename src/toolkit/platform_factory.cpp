#include "toolkit/application.hpp"
#include "toolkit/clipboard.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/xdg_image_loader.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <map>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#ifdef TOOLKIT_HAS_CAIRO
#include "platform/macos/macos_cairo_platform.hpp"
#endif
#include "platform/macos/macos_native_platform.hpp"
#include "platform/macos/macos_opengl_platform.hpp"
#elif defined(_WIN32)
#include "platform/win32/win32_platform.hpp"
#else
#include <unistd.h>
#ifdef TOOLKIT_HAS_X11
#include "platform/x11/x11_platform.hpp"
#endif
#ifdef TOOLKIT_HAS_WAYLAND
#include "platform/wayland/wl_platform.hpp"
#endif
#endif


namespace toolkit {

bool is_wayland_session() { return std::getenv("WAYLAND_DISPLAY") != nullptr; }

class DummyIconProvider : public IconProvider {
  public:
    auto load(std::string_view, int, std::string_view) -> Icon override { return nullptr; }
};

static PlatformApplication *s_platform = nullptr;
static Application *s_application = nullptr;

// FIXME: remove the detail namespace
namespace detail {
PlatformApplication *current_platform() { return s_platform; }
void set_current_platform(PlatformApplication *p) { s_platform = p; }
Application *current_application() { return s_application; }
void set_current_application(Application *a) { s_application = a; }
} // namespace detail

bool Application::has_instance() { return s_application != nullptr; }

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
    if (is_wayland_session()) {
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
    std::string application_name;
};

static std::string get_executable_name() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring path(buffer);
    size_t last_slash = path.find_last_of(L"\\/");
    std::wstring filename = (last_slash == std::wstring::npos) ? path : path.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of(L".");
    if (last_dot != std::wstring::npos) {
        filename = filename.substr(0, last_dot);
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, result.data(), len, nullptr, nullptr);
    return result;
#elif defined(__APPLE__)
    char buffer[1024];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        std::string path(buffer);
        size_t last_slash = path.find_last_of("/");
        return (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
    }
    return "svision3";
#else
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string path(buffer);
        size_t last_slash = path.find_last_of("/");
        return (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
    }
    return "svision3";
#endif
}

Application::Application() : impl_(std::make_unique<Impl>()) {
    if (const char *env_level = std::getenv("SVISION_LOG_LEVEL")) {
        set_log_level(env_level);
    }

    impl_->application_name = get_executable_name();
    detail::set_current_application(this);
    impl_->platform = create_platform_application();
    detail::set_current_platform(impl_->platform.get());

    impl_->icon_provider = std::make_unique<DummyIconProvider>();

    // Refresh theme now that platform is active to detect correct fonts/scale
    Theme::set_current(ThemeFactory::create(Theme::detect_system_style()));

    auto const &theme = Theme::current();
    spdlog::info("Theme: {} (Font: '{}' {}px, Monospace: '{}', Scale: {:.2f})", theme.name,
                 theme.palette.fonts.system, theme.palette.fonts.size,
                 theme.palette.fonts.monospace, impl_->platform->scale_factor());
}

Application::~Application() {
    detail::set_current_platform(nullptr);
    detail::set_current_application(nullptr);
}

Application &Application::instance() { return *detail::current_application(); }

bool Application::set_log_level(std::string_view name) {
    auto numeric = 0;
    auto const [ptr, ec] = std::from_chars(name.data(), name.data() + name.size(), numeric);
    if (ec == std::errc{} && ptr == name.data() + name.size() && numeric >= spdlog::level::trace &&
        numeric < spdlog::level::n_levels) {
        spdlog::set_level(static_cast<spdlog::level::level_enum>(numeric));
        return true;
    }

    static constexpr std::string_view valid_names[] = {
        "trace", "debug", "info", "warning", "warn", "error", "err", "critical", "off",
    };
    auto known = std::find(std::begin(valid_names), std::end(valid_names), name) !=
                std::end(valid_names);
    if (!known) {
        spdlog::warn("Application::set_log_level: unknown level '{}' (expected 0-6, or one of: "
                     "trace, debug, info, warning, error, critical, off)",
                     name);
        return false;
    }
    spdlog::set_level(spdlog::level::from_str(std::string(name)));
    return true;
}

bool platformNeedsCSD() {
    // FIXME: what other sessions must have CSD?
    auto xdg = std::getenv("XDG_CURRENT_DESKTOP");
    auto is_gnome = false;
    if (xdg) {
        std::string s(xdg);
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s.find("gnome") != std::string::npos) {
            is_gnome = true;
        }
    }
    auto is_wayland = std::getenv("WAYLAND_DISPLAY") != nullptr;
    return is_gnome && is_wayland;
}

Window *Application::create_window(std::string_view title, Size size, WindowOptions options) {
    if (platformNeedsCSD()) {
        options.csd = true;
    }
    windows_.push_back(std::make_unique<Window>(title, size, options));
    return windows_.back().get();
}

int Application::run() { return impl_->platform->run(); }
void Application::run_until(std::function<bool()> should_exit) {
    impl_->platform->run_until(std::move(should_exit));
}
void Application::quit() { impl_->platform->quit(); }

void Application::set_application_name(std::string_view name) { impl_->application_name = name; }

std::string_view Application::application_name() const { return impl_->application_name; }

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
    } else {
        spdlog::error("Failed loading {} size {}", icon_name, size);
        // Negative cache: store an empty image so we don't try to load it again
        impl_->icon_cache[key] = std::make_shared<ImageData>();
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
