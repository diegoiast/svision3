#include "svision3/image_widget.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/widget_loader.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace svision3;

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

// Scrollbar geometry is derived from the widget's rect, and ScrollableWidget did
// not recompute it on resize: the bars kept whatever position they were given at
// the last content change. An image set before the widget was ever laid out left
// them parked at a negative offset, outside the widget entirely.
TEST_CASE("ImageWidget keeps its scrollbars at the edges across resizes",
          "[image_widget][scrollbars]") {
    // Win95 draws real scrollbar child widgets rather than overlay thumbs, so
    // their rects are observable through for_each_child.
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));

    auto image = std::make_shared<ImageData>();
    image->width = 2000;
    image->height = 2000;
    image->pixels.resize(size_t{2000} * 2000 * 4);

    ImageWidget w;
    w.set_image(image); // content arrives before any layout, as it does on open

    // The vertical bar is the tall one; the horizontal bar is the wide one.
    auto bars = [&w] {
        auto v = Rect{};
        auto h = Rect{};
        w.for_each_child([&](Widget *child) {
            if (!child->is_visible()) {
                return;
            }
            if (child->rect().height > child->rect().width) {
                v = child->rect();
            } else {
                h = child->rect();
            }
        });
        return std::pair{v, h};
    };

    for (auto width : {300.0f, 600.0f, 250.0f}) {
        w.set_rect({0, 0, width, 400});
        auto [v, h] = bars();

        INFO("widget width " << width);
        REQUIRE(v.width > 0);
        REQUIRE(h.height > 0);

        // The vertical bar hugs the right edge, the horizontal one the bottom.
        REQUIRE(v.x > 0);
        REQUIRE(v.x + v.width <= width);
        REQUIRE(v.x + v.width > width - 20.0f);
        REQUIRE(h.y + h.height <= 400.0f);
        REQUIRE(h.y + h.height > 380.0f);
    }
}
