#include <catch2/catch_test_macros.hpp>
#include "svision3/slider.hpp"
#include "svision3/layout.hpp"

using namespace svision3;

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
    Slider s(Orientation::Horizontal);
    s.set_range(0, 100);
    s.set_rect({0, 0, 100, 20});
    
    float last_val = -1.0f;
    s.on_change = [&](auto &s, float v) { last_val = v; };
    
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

TEST_CASE("Slider interaction in layout", "[slider]") {
    auto layout = std::make_unique<VBoxLayout>();
    layout->set_rect({10, 10, 200, 200});
    
    auto s_ptr = std::make_unique<Slider>(Orientation::Horizontal);
    auto s = s_ptr.get();
    s->set_range(0, 100);
    layout->add_widget(std::move(s_ptr));
    layout->set_rect({10, 10, 200, 200}); // This triggers apply_layout
    REQUIRE(s->rect().width == 200.0f);
    REQUIRE(s->rect().height > 0);
    REQUIRE(s->rect().x == 0.0f);
    REQUIRE(s->rect().y == 0.0f);
    
    // Simulate Window::handle_mouse capture logic
    // Window receives Press at {20, 20} (local to Window)
    // Layout is at {10, 10}, so local to Layout is {10, 10}
    // Slider is at {0, 0} relative to Layout, so local to Slider is {10, 10}
    
    Point window_press = {110, 20};
    Point local_press = s->map_from_window(window_press);
    REQUIRE(local_press.x == 100.0f);
    REQUIRE(local_press.y == 10.0f);
    
    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = local_press;
    bool handled = s->handle_mouse(press);
    REQUIRE(handled == true);
    REQUIRE(s->value() > 40.0f); // Roughly 50%
    REQUIRE(s->value() < 60.0f); 
    
    // Drag to window {210, 20} -> should be Slider {200, 10}
    Point window_drag = {210, 20};
    Point local_drag = s->map_from_window(window_drag);
    REQUIRE(local_drag.x == 200.0f);
    
    MouseEvent drag{};
    drag.type = MouseEvent::Type::Drag;
    drag.position = local_drag;
    s->handle_mouse(drag);
    REQUIRE(s->value() == 100.0f); // Maxed out at x=200
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
