#include "toolkit/label.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("Widget defaults", "[widget]") {
    Label w("test");
    REQUIRE(w.is_enabled() == true);
    REQUIRE(w.is_visible() == true);
    REQUIRE(w.is_focused() == false);
    REQUIRE(w.is_focusable() == false);
    REQUIRE(w.cursor() == CursorShape::Arrow);
    REQUIRE(w.window() == nullptr);
}

TEST_CASE("Widget enable/disable", "[widget]") {
    Label w("test");
    w.set_enabled(false);
    REQUIRE(w.is_enabled() == false);
    w.set_enabled(true);
    REQUIRE(w.is_enabled() == true);
}

TEST_CASE("Widget show/hide", "[widget]") {
    Label w("test");
    REQUIRE(w.is_visible() == true);
    w.hide();
    REQUIRE(w.is_visible() == false);
    w.show();
    REQUIRE(w.is_visible() == true);
    w.set_visible(false);
    REQUIRE(w.is_visible() == false);
}

TEST_CASE("Widget focused state", "[widget]") {
    Label w("test");
    w.set_focused(true);
    REQUIRE(w.is_focused() == true);
    w.set_focused(false);
    REQUIRE(w.is_focused() == false);
}

TEST_CASE("Widget min/max size", "[widget]") {
    Label w("test");
    REQUIRE(w.min_size().width == 0);
    REQUIRE(w.min_size().height == 0);
    REQUIRE(w.max_size().width == 0);
    REQUIRE(w.max_size().height == 0);

    w.set_min_size({100, 50});
    REQUIRE(w.min_size().width == 100);
    REQUIRE(w.min_size().height == 50);

    w.set_max_size({400, 300});
    REQUIRE(w.max_size().width == 400);
    REQUIRE(w.max_size().height == 300);
}

TEST_CASE("Widget coordinate mapping", "[widget]") {
    Label parent("Parent");
    parent.set_rect({10, 20, 100, 50});
    
    Label child("Child");
    child.set_rect({5, 5, 20, 20});
    child.set_parent(&parent);
    
    // map_to_window
    // local child {0,0} -> window {15, 25} (10+5, 20+5)
    Point p1 = child.map_to_window({0, 0});
    REQUIRE(p1.x == 15.0f);
    REQUIRE(p1.y == 25.0f);
    
    // map_from_window
    // window {15, 25} -> local child {0,0}
    Point p2 = child.map_from_window({15, 25});
    REQUIRE(p2.x == 0.0f);
    REQUIRE(p2.y == 0.0f);
    
    // window {20, 30} -> local child {5, 5}
    Point p3 = child.map_from_window({20, 30});
    REQUIRE(p3.x == 5.0f);
    REQUIRE(p3.y == 5.0f);
}

TEST_CASE("Widget rect and hit_test", "[widget]") {
    Label w("test");
    w.set_rect({10, 20, 100, 50});
    REQUIRE(w.rect().x == 10);
    REQUIRE(w.rect().y == 20);
    REQUIRE(w.rect().width == 100);
    REQUIRE(w.rect().height == 50);

    // Label uses relative coordinates by default now.
    // So hit_test({5, 5}) should be true (it's inside 0..100, 0..50)
    REQUIRE(w.hit_test({5, 5}) == true);

    // hit_test({50, 40}) is also true
    REQUIRE(w.hit_test({50, 40}) == true);

    // hit_test({-1, -1}) should be false
    REQUIRE(w.hit_test({-1, -1}) == false);
}

TEST_CASE("Hidden widget excluded from find_focusable_at", "[widget]") {
    Label w("test");
    w.set_rect({0, 0, 100, 100});
    w.hide();
    REQUIRE(w.find_focusable_at({50, 50}) == nullptr);
}

TEST_CASE("Hidden widget excluded from widget_at", "[widget]") {
    Label w("test");
    w.set_rect({0, 0, 100, 100});
    w.hide();
    REQUIRE(w.widget_at({50, 50}) == nullptr);
}

TEST_CASE("Widget tooltip default is empty", "[widget]") {
    Label w("test");
    REQUIRE(w.tooltip().empty());
}

TEST_CASE("Widget set_tooltip stores text", "[widget]") {
    Label w("test");
    w.set_tooltip("hello");
    REQUIRE(w.tooltip() == "hello");
}

TEST_CASE("Widget tooltip can be updated", "[widget]") {
    Label w("test");
    w.set_tooltip("first");
    REQUIRE(w.tooltip() == "first");
    w.set_tooltip("second");
    REQUIRE(w.tooltip() == "second");
}

TEST_CASE("Widget tooltip can be cleared", "[widget]") {
    Label w("test");
    w.set_tooltip("something");
    REQUIRE(!w.tooltip().empty());
    w.set_tooltip("");
    REQUIRE(w.tooltip().empty());
}
