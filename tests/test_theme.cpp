#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
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

TEST_CASE("theme_style_count matches enum", "[theme]") { REQUIRE(theme_style_count == 7); }

TEST_CASE("ThemeFactory::crete produces named theme", "[theme]") {
    auto t = ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light);
    REQUIRE(t->name == "macOS");
}

TEST_CASE("ThemeFactory::crete with all styles and schemes", "[theme]") {
    for (int i = 0; i < theme_style_count; i++) {
        auto style = static_cast<ThemeStyle>(i);
        for (auto scheme : {ColorScheme::Light, ColorScheme::Dark}) {
            auto t = ThemeFactory::create(style, scheme);
            REQUIRE_FALSE(t->name.empty());
        }
    }
}

TEST_CASE("Theme::default_palette returns valid palette", "[theme]") {
    auto t = ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light);
    auto p = t->default_palette(ColorScheme::Light);
    REQUIRE(p.fonts.size > 0);
    REQUIRE(p.corner_radius >= 0);
    REQUIRE(p.border_width >= 0);
    REQUIRE(p.text.a == 1.0f);
}

TEST_CASE("Win95 palette has beveled flag", "[theme]") {
    auto t = ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light);
    auto p = t->default_palette(ColorScheme::Light);
    REQUIRE(p.beveled == true);
}

TEST_CASE("Non-Win95 palettes are not beveled", "[theme]") {
    for (auto s : {ThemeStyle::MacOS, ThemeStyle::Material, ThemeStyle::Win11, ThemeStyle::Plasma6,
                   ThemeStyle::GNOME}) {
        auto t = ThemeFactory::create(s, ColorScheme::Light);
        auto p = t->default_palette(ColorScheme::Light);
        REQUIRE(p.beveled == false);
    }
}

TEST_CASE("Theme::set_current / current round-trip", "[theme]") {
    auto t = ThemeFactory::create(ThemeStyle::Material, ColorScheme::Dark);
    Theme::set_current(std::move(t));
    REQUIRE(Theme::current().name == "Material");
}

TEST_CASE("Dark theme has lighter text than background", "[theme]") {
    auto t = ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark);
    auto text_luma = t->palette.text.luma();
    auto bg_luma = t->palette.window.luma();
    REQUIRE(text_luma > bg_luma);
}

TEST_CASE("Light theme has darker text than background", "[theme]") {
    auto t = ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light);
    float text_luma = t->palette.text.luma();
    float bg_luma = t->palette.window.luma();
    REQUIRE(text_luma < bg_luma);
}


TEST_CASE("ProgressBar style has Win95 chunked", "[theme]") {
    auto t = ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light);
    REQUIRE(t->palette.progress_bar_height == 20.0f);
}

TEST_CASE("Theme has correct inline_scrollbars defaults", "[theme]") {
    // Regular scrollbars for Win95 and GNOME
    REQUIRE(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light)->palette.inline_scrollbars == false);
    REQUIRE(ThemeFactory::create(ThemeStyle::GNOME, ColorScheme::Light)->palette.inline_scrollbars == false);

    // Inline scrollbars for others
    REQUIRE(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light)->palette.inline_scrollbars == true);
    REQUIRE(ThemeFactory::create(ThemeStyle::Material, ColorScheme::Light)->palette.inline_scrollbars == true);
    REQUIRE(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light)->palette.inline_scrollbars == true);
    REQUIRE(ThemeFactory::create(ThemeStyle::Plasma6, ColorScheme::Light)->palette.inline_scrollbars == true);
}
