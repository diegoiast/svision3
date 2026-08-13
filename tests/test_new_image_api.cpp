// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/image.hpp"
#include "svision3/platform.hpp"
#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>

using namespace svision3;

TEST_CASE("New Image API", "[image]") {
    auto app = create_platform_application();
    auto loader = app->get_image_loader();

    SECTION("Supported extensions") {
        auto ext = loader->supported_extensions();
        CHECK_FALSE(ext.empty());
        CHECK(std::find(ext.begin(), ext.end(), ".png") != ext.end());
    }

    SECTION("Load non-existent file") {
        auto img = loader->load("non_existent_file.png");
        CHECK(img == nullptr);
    }

    SECTION("Load real image") {
        std::string test_image = "themes/Faenza/actions/16/gtk-edit.png";
        auto img = loader->load(test_image);
        REQUIRE(img != nullptr);
        CHECK(img->width == 16);
        CHECK(img->height == 16);
    }

    SECTION("Load from memory") {
        std::string test_image = "themes/Faenza/actions/16/gtk-edit.png";
        std::ifstream file(test_image, std::ios::binary);
        REQUIRE(file.is_open());
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());

        auto img = loader->load_from_memory(data.data(), data.size());
        REQUIRE(img != nullptr);
        CHECK(img->width == 16);
        CHECK(img->height == 16);
    }
}
