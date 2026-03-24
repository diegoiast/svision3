#include "toolkit/image_loader.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("ImageLoader::load returns image data", "[image]") {
    ImageLoader loader;

    auto img = loader.load("themes/Faenza/actions/16/gtk-edit.png");
    REQUIRE(img.has_value());
    REQUIRE(img->width > 0);
    REQUIRE(img->height > 0);
    REQUIRE(img->channels == 4);
    REQUIRE(!img->pixels.empty());
}

TEST_CASE("ImageLoader::load returns nullopt for invalid path", "[image]") {
    ImageLoader loader;

    auto img = loader.load("nonexistent/image.png");
    REQUIRE(!img.has_value());
}

TEST_CASE("ImageLoader::supported_extensions", "[image]") {
    ImageLoader loader;

    auto exts = loader.supported_extensions();
    REQUIRE(exts.size() > 0);
    REQUIRE(std::find(exts.begin(), exts.end(), ".png") != exts.end());
}

TEST_CASE("XdgImageLoader loads action icon", "[image]") {
    XdgImageLoader loader("Faenza");

    auto img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(img.has_value());
    REQUIRE(img->width == 16);
    REQUIRE(img->height == 16);
}

TEST_CASE("XdgImageLoader loads different sizes", "[image]") {
    XdgImageLoader loader("Faenza");

    auto img16 = loader.load("gtk-edit", 16, "actions");
    auto img32 = loader.load("gtk-edit", 32, "actions");
    auto img48 = loader.load("gtk-edit", 48, "actions");

    REQUIRE(img16->width == 16);
    REQUIRE(img32->width == 32);
    REQUIRE(img48->width == 48);
}

TEST_CASE("XdgImageLoader::set_theme changes theme", "[image]") {
    XdgImageLoader loader("Faenza");

    loader.set_theme("NonExistent");
    auto img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(!img.has_value());

    loader.set_theme("Faenza");
    img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(img.has_value());
}

TEST_CASE("XdgImageLoader loads icons from different contexts", "[image]") {
    XdgImageLoader loader("Faenza");

    auto img = loader.load("gtk-edit", 16, "actions");
    REQUIRE(img.has_value());

    img = loader.load("document-open", 16, "actions");
    REQUIRE(img.has_value());
}