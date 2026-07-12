#include "toolkit/lunasvg_image_loader.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/xdg_image_loader.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

namespace {

// Mirrors the real Breeze/KDE "-symbolic" convention: fill="currentColor" resolved via a
// `.ColorScheme-Text { color: ... }` rule in a `<style id="current-color-scheme">` block. Real
// theme files bake in a color for their intended panel background (e.g. breeze-dark bakes in a
// near-white #fcfcfc), which should be overridden to match svision3's live theme rather than
// rendered as-is.
constexpr auto SYMBOLIC_SVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <defs>
    <style type="text/css" id="current-color-scheme">.ColorScheme-Text { color: #fcfcfc; }</style>
  </defs>
  <path style="fill:currentColor;fill-opacity:1;stroke:none" class="ColorScheme-Text"
        d="M 2 2 L 2 14 L 14 14 L 14 2 L 2 2 z"/>
</svg>)";

constexpr auto PLAIN_SVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="#123456" d="M 2 2 L 2 14 L 14 14 L 14 2 L 2 2 z"/>
</svg>)";

// Mirrors the real Breeze/KDE status icon (e.g. /usr/share/icons/breeze/status/16/dialog-error.svg):
// a solid ColorScheme-NegativeText background rect with a hardcoded white glyph on top. Guards
// against the R/B channel swap bug where LunasvgImageLoader copied lunasvg's native
// ARGB32_Premultiplied bytes (B,G,R,A in memory) straight into ImageData::pixels without
// converting to the toolkit's R,G,B,A convention -- red icons like this one rendered blue.
constexpr auto STATUS_SVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <style type="text/css" id="current-color-scheme">.ColorScheme-NegativeText { color:#da4453; }</style>
  <rect class="ColorScheme-NegativeText" x="2" y="2" width="12" height="12" fill="currentColor"/>
  <rect x="7" y="7" width="2" height="2" fill="#fff"/>
</svg>)";

auto center_pixel(ImageData const &img) -> std::array<uint8_t, 4> {
    auto idx = static_cast<size_t>((img.height / 2) * img.width + img.width / 2) * 4;
    return {img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2], img.pixels[idx + 3]};
}

} // namespace

TEST_CASE("LunasvgImageLoader recolors ColorScheme-* symbolic icons to the active theme",
         "[image]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    auto const &text = Theme::current().palette.text;

    LunasvgImageLoader loader;
    auto img = loader.load_svg_from_memory(reinterpret_cast<const uint8_t *>(SYMBOLIC_SVG),
                                           std::char_traits<char>::length(SYMBOLIC_SVG), 16, 16);
    REQUIRE(img);

    auto px = center_pixel(*img);
    REQUIRE(px[0] == static_cast<uint8_t>(text.r * 255.0f + 0.5f));
    REQUIRE(px[1] == static_cast<uint8_t>(text.g * 255.0f + 0.5f));
    REQUIRE(px[2] == static_cast<uint8_t>(text.b * 255.0f + 0.5f));
    // Must not still be the theme file's own baked-in default (#fcfcfc).
    REQUIRE_FALSE((px[0] == 0xfc && px[1] == 0xfc && px[2] == 0xfc));
}

TEST_CASE("LunasvgImageLoader does not swap red and blue on a colored status icon", "[image]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    auto const &err = Theme::current().palette.error;

    LunasvgImageLoader loader;
    auto img = loader.load_svg_from_memory(reinterpret_cast<const uint8_t *>(STATUS_SVG),
                                           std::char_traits<char>::length(STATUS_SVG), 16, 16);
    REQUIRE(img);

    // Sample a corner of the background rect, away from the white glyph in the center.
    auto idx = static_cast<size_t>(4 * img->width + 4) * 4;
    REQUIRE(img->pixels[idx + 0] == static_cast<uint8_t>(err.r * 255.0f + 0.5f));
    REQUIRE(img->pixels[idx + 1] == static_cast<uint8_t>(err.g * 255.0f + 0.5f));
    REQUIRE(img->pixels[idx + 2] == static_cast<uint8_t>(err.b * 255.0f + 0.5f));
}

TEST_CASE("LunasvgImageLoader leaves icons without ColorScheme classes unchanged", "[image]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark));

    LunasvgImageLoader loader;
    auto img = loader.load_svg_from_memory(reinterpret_cast<const uint8_t *>(PLAIN_SVG),
                                           std::char_traits<char>::length(PLAIN_SVG), 16, 16);
    REQUIRE(img);

    auto px = center_pixel(*img);
    REQUIRE(px[0] == 0x12);
    REQUIRE(px[1] == 0x34);
    REQUIRE(px[2] == 0x56);
}

TEST_CASE("LunasvgImageLoader::load returns image data", "[image]") {
    LunasvgImageLoader loader;

    auto img = loader.load("themes/Faenza/actions/scalable/add-files-to-archive.svg");
    REQUIRE(img);
    REQUIRE(img->width > 0);
    REQUIRE(img->height > 0);
    REQUIRE(img->channels == 4);
    REQUIRE(!img->pixels.empty());
}

TEST_CASE("XdgImageLoader loads SVG icon", "[image]") {
    XdgImageLoader loader("Faenza");

    // add-files-to-archive is in actions/scalable
    auto img = loader.load("add-files-to-archive", 32, "actions");
    REQUIRE(img);
    REQUIRE(img->width == 32);
    REQUIRE(img->height == 32);
}

/*
TEST_CASE("StbImageLoader::load returns image data", "[image]") {
    StbImageLoader loader;

    auto img = loader.load("themes/Faenza/actions/16/gtk-edit.png");
    REQUIRE(img);
    REQUIRE(img->width > 0);
    REQUIRE(img->height > 0);
    REQUIRE(img->channels == 4);
    REQUIRE(!img->pixels.empty());
}

TEST_CASE("StbImageLoader::load returns nullopt for invalid path", "[image]") {
    StbImageLoader loader;

    auto img = loader.load("nonexistent/image.png");
    REQUIRE(!img);
}

TEST_CASE("StbImageLoader::supported_extensions", "[image]") {
    StbImageLoader loader;

    auto exts = loader.supported_extensions();
    REQUIRE(exts.size() > 0);
    REQUIRE(std::find(exts.begin(), exts.end(), ".png") != exts.end());
}
*/

TEST_CASE("XdgImageLoader loads action icon", "[image]") {
    XdgImageLoader loader("Faenza");

    auto img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(img);
    REQUIRE(img->width == 16);
    REQUIRE(img->height == 16);
}

TEST_CASE("XdgImageLoader loads different sizes", "[image]") {
    XdgImageLoader loader("Faenza");

    auto img16 = loader.load("gtk-edit", 16, "actions");
    auto img32 = loader.load("gtk-edit", 32, "actions");
    auto img48 = loader.load("gtk-edit", 48, "actions");

    // This should not fail, ideally on the unit tests we would have a proper theme
    if (img16) {
        REQUIRE(img16->width == 16);
    }
    if (img32) {
        REQUIRE(img32->width == 32);
    }
    if (img48) {
        REQUIRE(img48->width == 48);
    }
}

TEST_CASE("XdgImageLoader::set_theme changes theme", "[image]") {
    XdgImageLoader loader("Faenza");

    loader.set_theme("NonExistent");
    auto img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(!img);

    loader.set_theme("Faenza");
    img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(img);
}

TEST_CASE("XdgImageLoader loads icons from different contexts", "[image]") {
    XdgImageLoader loader("Faenza");

    auto img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(img);

    img = loader.load("document-open", 16, "actions");
    REQUIRE(img);
}
