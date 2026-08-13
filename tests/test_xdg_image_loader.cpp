#include "svision3/lunasvg_image_loader.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/xdg_image_loader.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

// Test the raster loader that the target platform actually ships: WIC/GDI+ on Windows,
// stb elsewhere. Both produce B,G,R,A when asked, so the round-trip assertions hold.
#ifdef _WIN32
#include "svision3/platform/win32/win32_image_loader.hpp"
// clang-format off
#include <windows.h>
#include <gdiplus.h>
// clang-format on
#else
#include "svision3/stb_image_loader.hpp"
#endif

using namespace svision3;

#ifdef _WIN32
using RasterLoader = Win32ImageLoader;
#else
using RasterLoader = StbImageLoader;
#endif

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
// against red/blue channel mixups at the ImageData::pixels (B,G,R,A) boundary.
constexpr auto STATUS_SVG = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <style type="text/css" id="current-color-scheme">.ColorScheme-NegativeText { color:#da4453; }</style>
  <rect class="ColorScheme-NegativeText" x="2" y="2" width="12" height="12" fill="currentColor"/>
  <rect x="7" y="7" width="2" height="2" fill="#fff"/>
</svg>)";

auto center_pixel(ImageData const &img) -> std::array<uint8_t, 4> {
    auto idx = static_cast<size_t>((img.height / 2) * img.width + img.width / 2) * 4;
    return {img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2], img.pixels[idx + 3]};
}

// A minimal 2x2 PNG, one solid color per pixel (top-left=red, top-right=green,
// bottom-left=blue, bottom-right=white), embedded so this test has no filesystem dependency and
// no reliance on any real icon theme being installed. Exercises all three channels independently
// so a channel permutation bug (e.g. R and B swapped, or G accidentally swapped with something)
// can't hide behind a single-color test. Generated with:
//
//   from PIL import Image
//   import io
//   img = Image.new('RGBA', (2, 2))
//   img.putpixel((0, 0), (255, 0, 0, 255))    # top-left: red
//   img.putpixel((1, 0), (0, 255, 0, 255))    # top-right: green
//   img.putpixel((0, 1), (0, 0, 255, 255))    # bottom-left: blue
//   img.putpixel((1, 1), (255, 255, 255, 255))  # bottom-right: white
//   buf = io.BytesIO()
//   img.save(buf, format='PNG')
//   print(', '.join(f'0x{b:02x}' for b in buf.getvalue()))
constexpr uint8_t RGBW_PNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d,
    0x24, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x05, 0xc1, 0x01, 0x0d, 0x00,
    0x00, 0x0c, 0xc3, 0x20, 0x96, 0xdc, 0xbf, 0xe5, 0x1e, 0x44, 0xd2, 0x4d, 0xc2, 0x03, 0x3e, 0xff,
    0x06, 0x00, 0x85, 0xd0, 0x93, 0x9c, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82,
};

auto pixel_at(ImageData const &img, int x, int y) -> std::array<uint8_t, 4> {
    auto idx = static_cast<size_t>(y * img.width + x) * 4;
    return {img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2], img.pixels[idx + 3]};
}

} // namespace

TEST_CASE("Raster loader loads a PNG as B,G,R,A and round-trips through save", "[image]") {
#ifdef _WIN32
    // Win32ImageLoader uses GDI+, which the real app inits in its ctor; do it here.
    ULONG_PTR gdiplus_token = 0;
    Gdiplus::GdiplusStartupInput gdiplus_input;
    Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr);
#endif
    RasterLoader loader;
    auto img = loader.load_from_memory(RGBW_PNG, sizeof(RGBW_PNG), PixelFormat::BGRA);
    REQUIRE(img);
    REQUIRE(img->width == 2);
    REQUIRE(img->height == 2);

    REQUIRE(pixel_at(*img, 0, 0) == std::array<uint8_t, 4>{0x00, 0x00, 0xff, 0xff}); // red
    REQUIRE(pixel_at(*img, 1, 0) == std::array<uint8_t, 4>{0x00, 0xff, 0x00, 0xff}); // green
    REQUIRE(pixel_at(*img, 0, 1) == std::array<uint8_t, 4>{0xff, 0x00, 0x00, 0xff}); // blue
    REQUIRE(pixel_at(*img, 1, 1) == std::array<uint8_t, 4>{0xff, 0xff, 0xff, 0xff}); // white

    auto tmp_path = std::filesystem::temp_directory_path() / "svision3_rgbw_roundtrip_test.png";
    REQUIRE(loader.save(*img, tmp_path.string()));

    auto roundtrip = loader.load(tmp_path.string(), PixelFormat::BGRA);
    std::filesystem::remove(tmp_path);
    REQUIRE(roundtrip);
    REQUIRE(pixel_at(*roundtrip, 0, 0) == std::array<uint8_t, 4>{0x00, 0x00, 0xff, 0xff});
    REQUIRE(pixel_at(*roundtrip, 1, 0) == std::array<uint8_t, 4>{0x00, 0xff, 0x00, 0xff});
    REQUIRE(pixel_at(*roundtrip, 0, 1) == std::array<uint8_t, 4>{0xff, 0x00, 0x00, 0xff});
    REQUIRE(pixel_at(*roundtrip, 1, 1) == std::array<uint8_t, 4>{0xff, 0xff, 0xff, 0xff});
#ifdef _WIN32
    Gdiplus::GdiplusShutdown(gdiplus_token);
#endif
}

TEST_CASE("LunasvgImageLoader recolors ColorScheme-* symbolic icons to the active theme",
         "[image]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    auto const &text = Theme::current().palette.text;

    LunasvgImageLoader loader;
    auto img = loader.load_svg_from_memory(reinterpret_cast<const uint8_t *>(SYMBOLIC_SVG),
                                           std::char_traits<char>::length(SYMBOLIC_SVG), 16, 16,
                                           PixelFormat::BGRA);
    REQUIRE(img);

    // ImageData::pixels is B,G,R,A.
    auto px = center_pixel(*img);
    REQUIRE(px[0] == static_cast<uint8_t>(text.b * 255.0f + 0.5f));
    REQUIRE(px[1] == static_cast<uint8_t>(text.g * 255.0f + 0.5f));
    REQUIRE(px[2] == static_cast<uint8_t>(text.r * 255.0f + 0.5f));
    // Must not still be the theme file's own baked-in default (#fcfcfc).
    REQUIRE_FALSE((px[0] == 0xfc && px[1] == 0xfc && px[2] == 0xfc));
}

TEST_CASE("LunasvgImageLoader does not swap red and blue on a colored status icon", "[image]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    auto const &err = Theme::current().palette.error;

    LunasvgImageLoader loader;
    auto img = loader.load_svg_from_memory(reinterpret_cast<const uint8_t *>(STATUS_SVG),
                                           std::char_traits<char>::length(STATUS_SVG), 16, 16,
                                           PixelFormat::BGRA);
    REQUIRE(img);

    // Sample a corner of the background rect, away from the white glyph in the center.
    // ImageData::pixels is B,G,R,A.
    auto idx = static_cast<size_t>(4 * img->width + 4) * 4;
    REQUIRE(img->pixels[idx + 0] == static_cast<uint8_t>(err.b * 255.0f + 0.5f));
    REQUIRE(img->pixels[idx + 1] == static_cast<uint8_t>(err.g * 255.0f + 0.5f));
    REQUIRE(img->pixels[idx + 2] == static_cast<uint8_t>(err.r * 255.0f + 0.5f));
}

TEST_CASE("LunasvgImageLoader leaves icons without ColorScheme classes unchanged", "[image]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Dark));

    LunasvgImageLoader loader;
    auto img = loader.load_svg_from_memory(reinterpret_cast<const uint8_t *>(PLAIN_SVG),
                                           std::char_traits<char>::length(PLAIN_SVG), 16, 16,
                                           PixelFormat::BGRA);
    REQUIRE(img);

    // ImageData::pixels is B,G,R,A.
    auto px = center_pixel(*img);
    REQUIRE(px[0] == 0x56);
    REQUIRE(px[1] == 0x34);
    REQUIRE(px[2] == 0x12);
}

TEST_CASE("LunasvgImageLoader::load returns image data", "[image]") {
    LunasvgImageLoader loader;

    // Self-authored fixture, not a real theme's copyrighted artwork -- reuses PLAIN_SVG above.
    auto img = loader.load_from_memory(reinterpret_cast<const uint8_t *>(PLAIN_SVG),
                                       std::char_traits<char>::length(PLAIN_SVG));
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
