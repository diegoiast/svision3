// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/label.hpp"
#include "svision3/painter.hpp"
#include "svision3/platform.hpp"
#include "svision3/platform/dummy_platform.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/window.hpp"
#include "svision3/window_title_bar.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace svision3;

namespace {
struct PlatformSwap {
    PlatformApplication *previous = detail::current_platform();
    DummyPlatformApplication dummy;

    PlatformSwap() { detail::set_current_platform(&dummy); }
    ~PlatformSwap() { detail::set_current_platform(previous); }
};

// Height the CSD title bar contributes on top of the actual content.
auto title_bar_height(Window &window, std::shared_ptr<Label> const &content) -> float {
    return window.content_min_size().height - content->size_hint().height;
}

// Counts filled circles so a test can tell a platform chevron from macOS' traffic-light dot.
class CircleCountingPainter : public Painter {
  public:
    int filled_circles = 0;

    void push_clip(Rect const &) override {}
    void pop_clip() override {}
    void push_translation(Point) override {}
    void pop_translation() override {}
    void push_rotation(float) override {}
    void pop_rotation() override {}
    void set_line_style(LineStyle) override {}
    void fill_rect(Rect const &, Color const &) override {}
    void draw_rect(Rect const &, Color const &, float) override {}
    void fill_rounded_rect(Rect const &, Color const &, float) override {}
    void draw_rounded_rect(Rect const &, Color const &, float, float) override {}
    void fill_triangle(Point, Point, Point, Color const &) override {}
    void draw_line(Point, Point, Color const &, float) override {}
    void fill_circle(Point, float, Color const &) override { filled_circles++; }
    void draw_circle(Point, float, Color const &, float) override {}
    void draw_image(ImageData const &, Point) override {}
    void draw_image_scaled(ImageData const &, Rect const &) override {}
    std::string_view name() const override { return "circle-counting"; }
};

auto minimize_button_fills_circle(Window &window) -> int {
    TitlebarButton button(DecorationButton::Minimize, "Minimize");
    button.set_window(&window);
    button.set_rect({0, 0, 20, 20});
    CircleCountingPainter painter;
    button.paint(painter);
    return painter.filled_circles;
}
} // namespace

TEST_CASE("WindowOptions decoration precedence", "[window][decoration]") {
    WindowOptions defaults{};
    REQUIRE(defaults.decoration == DecorationMode::Inherit);
    REQUIRE(requested_decoration(defaults) == DecorationMode::Inherit);

    WindowOptions legacy_csd{};
    legacy_csd.csd = true;
    REQUIRE(requested_decoration(legacy_csd) == DecorationMode::ThemeCsd);

    WindowOptions explicit_over_legacy{};
    explicit_over_legacy.csd = true;
    explicit_over_legacy.decoration = DecorationMode::Native;
    REQUIRE(requested_decoration(explicit_over_legacy) == DecorationMode::Native);

    WindowOptions explicit_platform{};
    explicit_platform.decoration = DecorationMode::PlatformCsd;
    REQUIRE(requested_decoration(explicit_platform) == DecorationMode::PlatformCsd);
}

TEST_CASE("Window defaults to native decorations", "[window][decoration]") {
    PlatformSwap platform;
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));

    Window window("Test", {400, 300});
    REQUIRE(window.decoration_mode() == DecorationMode::Native);
    REQUIRE_FALSE(window.options().csd);
}

TEST_CASE("PlatformCsd takes title bar geometry from the platform theme", "[window][decoration]") {
    PlatformSwap platform;
    // Win95 is deliberately distinct from every theme detect_system_style() can return, so the
    // two heights below cannot accidentally match.
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));
    auto platform_theme = ThemeFactory::create(Theme::detect_system_style(), ColorScheme::Light);
    auto const platform_bar = platform_theme->style.window_decoration.top;
    auto const theme_bar = Theme::current().style.window_decoration.top;
    REQUIRE(platform_bar != theme_bar);

    WindowOptions options{};
    options.decoration = DecorationMode::PlatformCsd;
    Window window("Test", {400, 300}, options);

    auto content = std::make_shared<Label>("hello");
    window.set_root(content);

    REQUIRE(window.decoration_mode() == DecorationMode::PlatformCsd);
    REQUIRE(window.decoration_theme().style.window_decoration.top == Catch::Approx(platform_bar));
    REQUIRE(title_bar_height(window, content) == Catch::Approx(platform_bar));
}

TEST_CASE("PlatformCsd keeps platform title bar geometry across theme changes",
          "[window][decoration]") {
    PlatformSwap platform;
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));
    auto platform_theme = ThemeFactory::create(Theme::detect_system_style(), ColorScheme::Light);
    auto const platform_bar = platform_theme->style.window_decoration.top;

    WindowOptions options{};
    options.decoration = DecorationMode::PlatformCsd;
    Window window("Test", {400, 300}, options);
    auto content = std::make_shared<Label>("hello");
    window.set_root(content);
    REQUIRE(title_bar_height(window, content) == Catch::Approx(platform_bar));
    auto const platform_window_color = window.decoration_theme().palette.window.to_argb32();

    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    auto const macos_bar = Theme::current().style.window_decoration.top;
    REQUIRE(macos_bar != platform_bar);

    REQUIRE(window.decoration_mode() == DecorationMode::PlatformCsd);
    REQUIRE(window.decoration_theme().style.window_decoration.top == Catch::Approx(platform_bar));
    REQUIRE(title_bar_height(window, content) == Catch::Approx(platform_bar));
    // Colours are the platform's too, so the app theme must not repaint the bar.
    REQUIRE(window.decoration_theme().palette.window.to_argb32() == platform_window_color);
}

TEST_CASE("PlatformCsd close button keeps its colour across theme changes",
          "[window][decoration]") {
    PlatformSwap platform;
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));
    WindowOptions options{};
    options.decoration = DecorationMode::PlatformCsd;
    Window window("Test", {400, 300}, options);
    auto const platform_error = window.decoration_theme().palette.error.to_argb32();

    Theme::set_current(ThemeFactory::create(ThemeStyle::Material, ColorScheme::Light));
    REQUIRE(Theme::current().palette.error.to_argb32() != platform_error);
    REQUIRE(window.decoration_theme().palette.error.to_argb32() == platform_error);
}

TEST_CASE("PlatformCsd paints title bar buttons with the platform theme", "[window][decoration]") {
    // On macOS the platform theme *is* the macOS theme, so there is no distinct glyph to check.
    if (Theme::detect_system_style() == ThemeStyle::MacOS) {
        SUCCEED("platform theme is macOS; nothing to distinguish");
        return;
    }

    PlatformSwap platform;
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    WindowOptions platform_options{};
    platform_options.decoration = DecorationMode::PlatformCsd;
    Window platform_window("Test", {400, 300}, platform_options);
    // macOS always draws minimize as a filled traffic-light dot; every other platform title bar
    // draws a chevron/line instead.
    REQUIRE(minimize_button_fills_circle(platform_window) == 0);

    WindowOptions theme_options{};
    theme_options.decoration = DecorationMode::ThemeCsd;
    Window theme_window("Test", {400, 300}, theme_options);
    REQUIRE(minimize_button_fills_circle(theme_window) > 0);
}

TEST_CASE("PlatformCsd title bar icon keeps its platform size across theme changes",
          "[window][decoration]") {
    // Only Plasma's title bar derives its icon slot from the title bar height; the other platform
    // title bars use fixed sizes, so they cannot regress this way.
    if (Theme::detect_system_style() != ThemeStyle::Plasma6) {
        SUCCEED("platform theme does not size its icon from the title bar height");
        return;
    }

    PlatformSwap platform;
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));
    WindowOptions options{};
    options.decoration = DecorationMode::PlatformCsd;
    Window window("Test", {400, 300}, options);
    auto const platform_top = window.decoration_theme().style.window_decoration.top;

    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    REQUIRE(Theme::current().style.window_decoration.top != platform_top);

    auto title_bar = window.decoration_theme().create_title_bar(&window);
    Size icon_size{};
    title_bar->for_each_child([&](Widget *child) {
        if (auto *icon = dynamic_cast<TitleBarIcon *>(child)) {
            icon_size = icon->min_size();
        }
    });
    REQUIRE(icon_size.height == Catch::Approx(platform_top - 4.0f));
}

TEST_CASE("Switching decoration mode rebuilds the CSD chrome", "[window][decoration]") {
    PlatformSwap platform;
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));
    auto platform_theme = ThemeFactory::create(Theme::detect_system_style(), ColorScheme::Light);
    auto const theme_bar = Theme::current().style.window_decoration.top;
    auto const platform_bar = platform_theme->style.window_decoration.top;

    WindowOptions options{};
    options.decoration = DecorationMode::ThemeCsd;
    Window window("Test", {400, 300}, options);
    auto content = std::make_shared<Label>("hello");
    window.set_root(content);

    REQUIRE(window.decoration_mode() == DecorationMode::ThemeCsd);
    REQUIRE(title_bar_height(window, content) == Catch::Approx(theme_bar));

    window.set_decoration_mode(DecorationMode::Native);
    REQUIRE(window.decoration_mode() == DecorationMode::Native);
    REQUIRE_FALSE(window.options().csd);
    REQUIRE(title_bar_height(window, content) == Catch::Approx(0.0f));

    window.set_decoration_mode(DecorationMode::PlatformCsd);
    REQUIRE(window.decoration_mode() == DecorationMode::PlatformCsd);
    REQUIRE(window.options().csd);
    REQUIRE(title_bar_height(window, content) == Catch::Approx(platform_bar));
}
