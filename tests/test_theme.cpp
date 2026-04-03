#include "toolkit/theme.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("Theme::style_name returns correct names", "[theme]") {
    REQUIRE(std::string(Theme::style_name(ThemeStyle::MacOS)) == "macOS");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Material)) == "Material");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Win11)) == "Windows 11");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Win95)) == "Windows 95");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::Plasma6)) == "Plasma 6");
    REQUIRE(std::string(Theme::style_name(ThemeStyle::GNOME)) == "GNOME");
}

TEST_CASE("theme_style_count matches enum", "[theme]") { REQUIRE(theme_style_count == 6); }

TEST_CASE("Theme::create produces named theme", "[theme]") {
    auto t = Theme::create(ThemeStyle::MacOS, ColorScheme::Light);
    REQUIRE(t->name == "macOS");
}

TEST_CASE("Theme::create with all styles and schemes", "[theme]") {
    for (int i = 0; i < theme_style_count; i++) {
        auto style = static_cast<ThemeStyle>(i);
        for (auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
            auto t = Theme::create(style, scheme);
            REQUIRE_FALSE(t->name.empty());
        }
    }
}

TEST_CASE("Theme::default_palette returns valid palette", "[theme]") {
    auto p = Theme::default_palette(ThemeStyle::MacOS, ColorScheme::Light);
    REQUIRE(p.fonts.size > 0);
    REQUIRE(p.corner_radius >= 0);
    REQUIRE(p.border_width >= 0);
    REQUIRE(p.text.a == 1.0f);
}

TEST_CASE("Win95 palette has beveled flag", "[theme]") {
    auto p = Theme::default_palette(ThemeStyle::Win95, ColorScheme::Light);
    REQUIRE(p.beveled == true);
}

TEST_CASE("Non-Win95 palettes are not beveled", "[theme]") {
    for (auto s : {ThemeStyle::MacOS, ThemeStyle::Material, ThemeStyle::Win11, ThemeStyle::Plasma6,
                   ThemeStyle::GNOME}) {
        auto p = Theme::default_palette(s, ColorScheme::Light);
        REQUIRE(p.beveled == false);
    }
}

TEST_CASE("Theme::set_current / current round-trip", "[theme]") {
    auto t = Theme::create(ThemeStyle::Material, ColorScheme::Dark);
    Theme::set_current(std::move(t));
    REQUIRE(Theme::current().name == "Material");
}

TEST_CASE("Dark theme has lighter text than background", "[theme]") {
    auto t = Theme::create(ThemeStyle::MacOS, ColorScheme::Dark);
    auto text_luma = t->palette.text.luma();
    auto bg_luma = t->palette.window.luma();
    REQUIRE(text_luma > bg_luma);
}

TEST_CASE("Light theme has darker text than background", "[theme]") {
    auto t = Theme::create(ThemeStyle::MacOS, ColorScheme::Light);
    float text_luma = t->palette.text.luma();
    float bg_luma = t->palette.window.luma();
    REQUIRE(text_luma < bg_luma);
}

TEST_CASE("Theme from custom palette", "[theme]") {
    Palette p;
    p.window = Color::rgb(0.1f, 0.1f, 0.1f);
    p.base = Color::rgb(0.2f, 0.2f, 0.2f);
    p.text = Color::rgb(0.9f, 0.9f, 0.9f);
    p.border = Color::rgb(0.5f, 0.5f, 0.5f);
    p.accent = Color::rgb(1.0f, 0.0f, 0.0f);
    p.alternate = Color::rgb(0.25f, 0.25f, 0.25f);
    p.fonts.size = 16.0f;

    auto t = Theme::create(ThemeStyle::MacOS, p);
    REQUIRE(t->palette.window.r == 0.1f);
    REQUIRE(t->palette.fonts.size == 16.0f);
}

TEST_CASE("ProgressBar style has Win95 chunked", "[theme]") {
    auto t = Theme::create(ThemeStyle::Win95, ColorScheme::Light);
    REQUIRE(t->palette.progress_bar_height == 20.0f);
}
