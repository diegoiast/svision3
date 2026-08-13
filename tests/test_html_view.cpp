// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/html_view.hpp"
#include "svision3/scroll_area.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

// ── helpers ───────────────────────────────────────────────────────────────────

static constexpr auto SIMPLE_HTML = R"(<!DOCTYPE html>
<html><body><p>Hello world</p></body></html>)";

static constexpr auto SIMPLE_MD = "# Hello\n\nSome **bold** text.\n";

static constexpr auto LIGHT_CSS = ".markdown-body { background: #fff; color: #000; }";
static constexpr auto DARK_CSS = ".markdown-body { background: #000; color: #fff; }";

// ── set_html ──────────────────────────────────────────────────────────────────

TEST_CASE("HtmlView set_html stores html", "[htmlview]") {
    HtmlView hv;
    hv.set_html(SIMPLE_HTML);
    REQUIRE(hv.html() == SIMPLE_HTML);
}

TEST_CASE("HtmlView set_html creates document with non-zero size hint", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_html(SIMPLE_HTML);
    auto hint = hv.size_hint();
    REQUIRE(hint.height > 0);
}

TEST_CASE("HtmlView set_html clears markdown so theme change does not re-render as markdown",
          "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});

    hv.set_markdown(SIMPLE_MD);
    auto html_after_markdown = hv.html();

    // Switch to raw HTML — markdown_ must be cleared
    hv.set_html(SIMPLE_HTML);
    REQUIRE(hv.html() == SIMPLE_HTML);

    // Simulate a theme change; html() should still be the raw HTML (not the re-rendered markdown)
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark));
    hv.on_theme_changed();
    REQUIRE(hv.html() == SIMPLE_HTML);
    (void)html_after_markdown;
}

// ── set_markdown ──────────────────────────────────────────────────────────────

TEST_CASE("HtmlView set_markdown produces a document", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_markdown(SIMPLE_MD);
    auto hint = hv.size_hint();
    REQUIRE(hint.height > 0);
}

TEST_CASE("HtmlView set_markdown keeps markdown for theme re-render", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_markdown(SIMPLE_MD);

    auto html_light = hv.html();

    // Switch to dark theme — on_theme_changed must re-render the markdown with dark colours
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark));
    hv.on_theme_changed();

    auto html_dark = hv.html();

    // The re-rendered HTML should differ (different background/foreground colours)
    REQUIRE(html_dark != html_light);
}

// ── set_css ───────────────────────────────────────────────────────────────────

TEST_CASE("HtmlView set_css triggers immediate re-render when markdown is active", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_markdown(SIMPLE_MD);

    auto html_no_css = hv.html();

    // Apply custom CSS — should immediately re-render
    hv.set_css(LIGHT_CSS, DARK_CSS);
    auto html_with_css = hv.html();

    // The HTML should now wrap content in .markdown-body
    REQUIRE(html_with_css.find("markdown-body") != std::string::npos);
    REQUIRE(html_with_css != html_no_css);
}

TEST_CASE("HtmlView set_css with empty strings removes custom CSS", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_css(LIGHT_CSS, DARK_CSS);
    hv.set_markdown(SIMPLE_MD);

    REQUIRE(hv.html().find("markdown-body") != std::string::npos);

    hv.set_css("", "");
    REQUIRE(hv.html().find("markdown-body") == std::string::npos);
}

TEST_CASE("HtmlView set_css does not re-render when no markdown is set", "[htmlview]") {
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_html(SIMPLE_HTML);

    auto html_before = hv.html();
    hv.set_css(LIGHT_CSS, DARK_CSS);
    // html_ must not change — no markdown to re-render
    REQUIRE(hv.html() == html_before);
}

// ── theme change ──────────────────────────────────────────────────────────────

TEST_CASE("HtmlView on_theme_changed picks correct css variant for dark theme", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_css(LIGHT_CSS, DARK_CSS);
    hv.set_markdown(SIMPLE_MD);

    auto html_light = hv.html();
    REQUIRE(html_light.find(LIGHT_CSS) != std::string::npos);

    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark));
    hv.on_theme_changed();
    auto html_dark = hv.html();

    REQUIRE(html_dark.find(DARK_CSS) != std::string::npos);
    REQUIRE(html_dark.find(LIGHT_CSS) == std::string::npos);
}

TEST_CASE("HtmlView on_theme_changed with no markdown just relayouts", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_rect({0, 0, 400, 300});
    hv.set_html(SIMPLE_HTML);

    auto html_before = hv.html();
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark));
    hv.on_theme_changed();
    // Raw HTML is not re-rendered on theme change
    REQUIRE(hv.html() == html_before);
}

// ── link click callback ───────────────────────────────────────────────────────

TEST_CASE("HtmlView on_link_click callback is stored", "[htmlview]") {
    HtmlView hv;
    std::string clicked;
    hv.on_link_click = [&](std::string const &url) { clicked = url; };
    hv.on_link_click("https://example.com");
    REQUIRE(clicked == "https://example.com");
}

// ── layout ────────────────────────────────────────────────────────────────────

TEST_CASE("HtmlView set_rect relayouts document when width changes", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    HtmlView hv;
    hv.set_html(SIMPLE_HTML);

    hv.set_rect({0, 0, 200, 300});
    auto narrow_height = hv.size_hint().height;

    hv.set_rect({0, 0, 800, 300});
    auto wide_height = hv.size_hint().height;

    // Wider viewport → less line-wrapping → shorter (or same) document height
    REQUIRE(wide_height <= narrow_height);
}

TEST_CASE("HtmlView inside ScrollArea receives on_theme_changed", "[htmlview]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    auto hv_owner = std::make_unique<HtmlView>();
    auto *hv = hv_owner.get();
    hv->set_css(LIGHT_CSS, DARK_CSS);
    hv->set_markdown(SIMPLE_MD);

    ScrollArea sa;
    sa.set_content(std::move(hv_owner));
    sa.set_rect({0, 0, 400, 300});

    auto html_light = hv->html();

    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark));
    sa.on_theme_changed();

    auto html_dark = hv->html();
    REQUIRE(html_dark != html_light);
    REQUIRE(html_dark.find(DARK_CSS) != std::string::npos);
}
