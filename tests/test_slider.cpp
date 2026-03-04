#include <catch2/catch_test_macros.hpp>
#include "toolkit/slider.hpp"

using namespace toolkit;

TEST_CASE("Slider default values", "[slider]") {
    Slider s;
    REQUIRE(s.minimum() == 0.0f);
    REQUIRE(s.maximum() == 100.0f);
    REQUIRE(s.value() == 0.0f);
}

TEST_CASE("Slider set_value", "[slider]") {
    Slider s;
    s.set_value(50.0f);
    REQUIRE(s.value() == 50.0f);
    
    // Test clamping
    s.set_value(150.0f);
    REQUIRE(s.value() == 100.0f);
    s.set_value(-50.0f);
    REQUIRE(s.value() == 0.0f);
}

TEST_CASE("Slider set_range", "[slider]") {
    Slider s;
    s.set_range(10.0f, 20.0f);
    REQUIRE(s.minimum() == 10.0f);
    REQUIRE(s.maximum() == 20.0f);
    REQUIRE(s.value() == 10.0f); // Should be clamped to new min
}

TEST_CASE("Slider mouse interaction", "[slider]") {
    Slider s(SliderOrientation::Horizontal);
    s.set_range(0, 100);
    s.set_rect({0, 0, 100, 20});
    
    float last_val = -1.0f;
    s.on_change = [&](float v) { last_val = v; };
    
    // Clicking at the middle (x=50) should set value to ~50
    // Note: handle_size affects the internal mapping, but with a 100px width
    // and default 16px handle, x=50 is roughly 50%.
    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 10};
    s.handle_mouse(press);
    
    REQUIRE(s.value() > 40.0f);
    REQUIRE(s.value() < 60.0f);
    REQUIRE(last_val == s.value());
    
    // Dragging to the right (x=100)
    MouseEvent drag{};
    drag.type = MouseEvent::Type::Drag;
    drag.position = {100, 10};
    s.handle_mouse(drag);
    REQUIRE(s.value() == 100.0f);
    
    // Releasing
    MouseEvent release{};
    release.type = MouseEvent::Type::Release;
    release.position = {100, 10};
    s.handle_mouse(release);
    
    // Programmatic change after release
    s.set_value(0.0f);
    REQUIRE(s.value() == 0.0f);
}

TEST_CASE("Slider keyboard interaction", "[slider]") {
    Slider s;
    s.set_range(0, 100);
    s.set_value(50);
    
    KeyEvent ev;
    ev.type = KeyEvent::Type::Press;
    
    // Right arrow -> Increase
    ev.key = Key::Right;
    s.handle_key(ev);
    REQUIRE(s.value() > 50.0f);
    
    // Left arrow -> Decrease
    float val = s.value();
    ev.key = Key::Left;
    s.handle_key(ev);
    REQUIRE(s.value() < val);
    
    // End -> Max
    ev.key = Key::End;
    s.handle_key(ev);
    REQUIRE(s.value() == 100.0f);
    
    // Home -> Min
    ev.key = Key::Home;
    s.handle_key(ev);
    REQUIRE(s.value() == 0.0f);
}
