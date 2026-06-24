#include "toolkit/lunasvg_image_loader.hpp"
#include "toolkit/xdg_image_loader.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

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
