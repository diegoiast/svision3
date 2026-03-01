#include <catch2/catch_test_macros.hpp>
#include "toolkit/theme.hpp"

using namespace toolkit;

TEST_CASE("Theme::style_name returns correct names", "[theme]") {
    REQUIRE(std::string(Theme::style_name(ThemeStyle::MacOS)) == "macOS");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Material)) == "Material");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Win11)) == "Windows 11");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Win95)) == "Windows 95");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Plasma6)) == "Plasma 6");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::GNOME)) == "GNOME");
}

TEST_CASE("theme_style_count matches enum", "[theme]") {
    REQUIRE(theme_style_count == 6);
}

TEST_CASE("Theme::create produces named theme", "[theme]") {
    auto t = Theme::create(ThemeStyle::MacOS, ColorScheme::Light);
    REQUIRE(t.name == "macOS");
}

TEST_CASE("Theme::create with all styles and schemes", "[theme]") {
    for (int i = 0; i < theme_style_count; i++) {
        auto style = static_cast<ThemeStyle>(i);
        for (auto scheme : {ColorScheme::Light, ColorScheme::Dark, ColorScheme::Pink}) {
            auto t = Theme::create(style, scheme);
            REQUIRE_FALSE(t.name.empty());
            REQUIRE(t.window.background.a == 1.0f);
            REQUIRE(t.button.font_size > 0);
            REQUIRE(t.label.font_size > 0);
        }
    }
}

TEST_CASE("Theme::default_palette returns valid palette", "[theme]") {
    auto p = Theme::default_palette(ThemeStyle::MacOS, ColorScheme::Light);
    REQUIRE(p.font_size > 0);
    REQUIRE(p.corner_radius >= 0);
    REQUIRE(p.border_width >= 0);
    REQUIRE(p.text.a == 1.0f);
}

TEST_CASE("Win95 palette has beveled flag", "[theme]") {
    auto p = Theme::default_palette(ThemeStyle::Win95, ColorScheme::Light);
    REQUIRE(p.beveled == true);
}

TEST_CASE("Non-Win95 palettes are not beveled", "[theme]") {
    for (auto s : {ThemeStyle::MacOS, ThemeStyle::Material, ThemeStyle::Win11,
                   ThemeStyle::Plasma6, ThemeStyle::GNOME}) {
        auto p = Theme::default_palette(s, ColorScheme::Light);
        REQUIRE(p.beveled == false);
    }
}

TEST_CASE("Theme::set_current / current round-trip", "[theme]") {
    auto t = Theme::create(ThemeStyle::Material, ColorScheme::Dark);
    Theme::set_current(t);
    REQUIRE(Theme::current().name == "Material");
}

TEST_CASE("Dark theme has lighter text than background", "[theme]") {
    auto t = Theme::create(ThemeStyle::MacOS, ColorScheme::Dark);
    float text_luma = 0.299f * t.label.text.r + 0.587f * t.label.text.g + 0.114f * t.label.text.b;
    float bg_luma = 0.299f * t.window.background.r + 0.587f * t.window.background.g + 0.114f * t.window.background.b;
    REQUIRE(text_luma > bg_luma);
}

TEST_CASE("Light theme has darker text than background", "[theme]") {
    auto t = Theme::create(ThemeStyle::MacOS, ColorScheme::Light);
    float text_luma = 0.299f * t.label.text.r + 0.587f * t.label.text.g + 0.114f * t.label.text.b;
    float bg_luma = 0.299f * t.window.background.r + 0.587f * t.window.background.g + 0.114f * t.window.background.b;
    REQUIRE(text_luma < bg_luma);
}

TEST_CASE("Theme from custom palette", "[theme]") {
    Palette p;
    p.window_bg = Color::rgb(0.1f, 0.1f, 0.1f);
    p.widget_bg = Color::rgb(0.2f, 0.2f, 0.2f);
    p.text = Color::rgb(0.9f, 0.9f, 0.9f);
    p.border = Color::rgb(0.5f, 0.5f, 0.5f);
    p.accent = Color::rgb(1.0f, 0.0f, 0.0f);
    p.alternate_bg = Color::rgb(0.25f, 0.25f, 0.25f);
    p.font_size = 16.0f;

    auto t = Theme::create(ThemeStyle::MacOS, p);
    REQUIRE(t.window.background.r == 0.1f);
    REQUIRE(t.label.font_size == 16.0f);
    REQUIRE(t.list_view.alternate_bg.r == 0.25f);
}

TEST_CASE("ProgressBar style has Win95 chunked", "[theme]") {
    auto t = Theme::create(ThemeStyle::Win95, ColorScheme::Light);
    REQUIRE(t.progress_bar.chunked == true);
    REQUIRE(t.progress_bar.bar_height == 20.0f);
}

TEST_CASE("Non-Win95 ProgressBar is not chunked", "[theme]") {
    auto t = Theme::create(ThemeStyle::MacOS, ColorScheme::Light);
    REQUIRE(t.progress_bar.chunked == false);
}
