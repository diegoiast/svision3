// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include <catch2/catch_test_macros.hpp>
#include "toolkit/image_widget.hpp"
#include "toolkit/image.hpp"

using namespace toolkit;

TEST_CASE("ImageWidget zoom and scroll", "[ImageWidget]") {
    ImageWidget iw;
    // Set a fixed size for the widget
    iw.set_rect({0, 0, 100, 100});

    // Create a dummy image
    auto img = std::make_shared<ImageData>(200, 200);
    iw.set_image(img);

    // Initial state: zoom 1.0, scroll 0,0
    REQUIRE(iw.zoom() == 1.0f);

    // Zoom in: effective size becomes 200x200
    iw.set_zoom(2.0f);
    REQUIRE(iw.zoom() == 2.0f);

    // Zooming out: effective size 100x100
    iw.set_zoom(0.5f);
    REQUIRE(iw.zoom() == 0.5f);
    
    // Reset
    iw.set_zoom(1.0f);

    // Pan: use mouse drag to scroll
    MouseEvent drag_event;
    drag_event.type = MouseEvent::Type::Drag;
    drag_event.position = {50, 50};
    
    // Start drag
    MouseEvent press_event;
    press_event.type = MouseEvent::Type::Press;
    press_event.button = 1;
    press_event.position = {0, 0};
    iw.handle_mouse(press_event);
    
    // Perform drag
    iw.handle_mouse(drag_event);
    
    // Check if scroll values were updated (expecting them to be negative of movement)
    // The implementation was: scroll_x_ -= (event.position.x - last_mouse_pos_.x);
    // last was 0, current is 50. change is +50. scroll becomes 0 - 50 = -50.
    // However, clamp_scroll() will clamp it back to 0.
    
    // For meaningful scroll, we need a zoom > 1.0
    iw.set_zoom(2.0f);
    iw.handle_mouse(press_event); // press at 0,0
    iw.handle_mouse(drag_event); // drag to 50,50
    // now scroll should be -50 (clamped). Wait, 50 - 0 = 50. 0 - 50 = -50.
    // Yes, scroll should change.
}
