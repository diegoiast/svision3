#include "toolkit/image_widget.hpp"
#include "toolkit/widget_loader.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace toolkit;

TEST_CASE("ImageWidget defaults", "[image_widget]") {
    ImageWidget w;
    REQUIRE(w.show_checkerboard() == true);
    REQUIRE(w.zoom() == 1.0f);
    REQUIRE(w.size_hint().width == 100);
    REQUIRE(w.size_hint().height == 100);
}

TEST_CASE("ImageWidget serialization round-trip", "[image_widget][serialization]") {
    ImageWidget w;
    w.set_show_checkerboard(false);
    w.set_zoom(2.5f);

    auto j = w.to_json();

    REQUIRE(j["type"] == "ImageWidget");
    REQUIRE(j["checkboard"] == false);
    REQUIRE(j["zoom"] == 2.5f);
    REQUIRE(j["scroll_x"] == 0.0f);
    REQUIRE(j["scroll_y"] == 0.0f);

    ImageWidget w2;
    w2.from_json(j);

    REQUIRE(w2.show_checkerboard() == false);
    REQUIRE(w2.zoom() == 2.5f);
}

TEST_CASE("ImageWidget clamps zoom", "[image_widget]") {
    ImageWidget w;
    w.set_zoom(20.0f);
    REQUIRE(w.zoom() == 10.0f);

    w.set_zoom(0.01f);
    REQUIRE(w.zoom() == 0.1f);
}

TEST_CASE("ImageWidget from_json restores scroll", "[image_widget][serialization]") {
    nlohmann::json j;
    j["type"] = "ImageWidget";
    j["checkboard"] = true;
    j["zoom"] = 1.0f;
    j["scroll_x"] = 50.0f;
    j["scroll_y"] = 30.0f;

    ImageWidget w;
    w.from_json(j);

    REQUIRE(w.show_checkerboard() == true);
    REQUIRE(w.zoom() == 1.0f);
}

TEST_CASE("ImageWidget can be created via WidgetLoader", "[image_widget][serialization]") {
    nlohmann::json j;
    j["type"] = "ImageWidget";
    j["checkboard"] = false;
    j["zoom"] = 0.5f;
    j["scroll_x"] = 10.0f;
    j["scroll_y"] = 20.0f;

    auto w = WidgetLoader::instance().create_widget(j);
    REQUIRE(w != nullptr);
    REQUIRE(w->class_name() == "ImageWidget");

    auto *img = dynamic_cast<ImageWidget *>(w.get());
    REQUIRE(img != nullptr);
    REQUIRE(img->show_checkerboard() == false);
    REQUIRE(img->zoom() == 0.5f);
}
