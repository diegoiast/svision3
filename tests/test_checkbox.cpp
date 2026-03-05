#include <catch2/catch_test_macros.hpp>
#include "toolkit/checkbox.hpp"

using namespace toolkit;

TEST_CASE("Checkbox default unchecked", "[checkbox]") {
    Checkbox cb("Option");
    REQUIRE(cb.checked() == false);
}

TEST_CASE("Checkbox set_checked", "[checkbox]") {
    Checkbox cb("Option");
    cb.set_checked(true);
    REQUIRE(cb.checked() == true);
    cb.set_checked(false);
    REQUIRE(cb.checked() == false);
}

TEST_CASE("Checkbox set_checked does not fire on_toggle", "[checkbox]") {
    Checkbox cb("Option");
    bool called = false;
    cb.on_toggle = [&](bool) { called = true; };
    cb.set_checked(true);
    REQUIRE(called == false);
    REQUIRE(cb.checked() == true);
}

TEST_CASE("Checkbox toggle via key fires on_toggle", "[checkbox]") {
    Checkbox cb("Option");
    cb.set_focused(true);
    bool toggled_value = false;
    cb.on_toggle = [&](bool v) { toggled_value = v; };
    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;
    ke.text = " ";
    cb.handle_key(ke);
    REQUIRE(toggled_value == true);
}

TEST_CASE("Checkbox is focusable", "[checkbox]") {
    Checkbox cb("Option");
    REQUIRE(cb.focusable() == true);
}

TEST_CASE("Checkbox toggles on space key", "[checkbox]") {
    Checkbox cb("Option");
    cb.set_focused(true);
    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;
    ke.text = " ";
    bool result = cb.handle_key(ke);
    REQUIRE(result == true);
    REQUIRE(cb.checked() == true);
}

TEST_CASE("Checkbox tri-state cycling", "[checkbox]") {
    Checkbox cb("Tri-state");
    cb.set_tri_state(true);
    cb.set_focused(true);
    
    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;
    ke.text = " ";
    
    // Default Unchecked
    REQUIRE(cb.check_state() == CheckState::Unchecked);
    
    // Toggle -> Checked
    cb.handle_key(ke);
    REQUIRE(cb.check_state() == CheckState::Checked);
    
    // Toggle -> Partial
    cb.handle_key(ke);
    REQUIRE(cb.check_state() == CheckState::Partial);
    
    // Toggle -> Unchecked
    cb.handle_key(ke);
    REQUIRE(cb.check_state() == CheckState::Unchecked);
}

TEST_CASE("Checkbox set_check_state", "[checkbox]") {
    Checkbox cb("Option");
    cb.set_check_state(CheckState::Partial);
    REQUIRE(cb.check_state() == CheckState::Partial);
    REQUIRE(cb.checked() == false);
}

TEST_CASE("Checkbox relative coordinates", "[checkbox]") {
    Checkbox cb("Option");
    cb.set_rect({100, 100, 200, 30});
    
    REQUIRE(cb.use_relative_coordinates() == true);
    
    MouseEvent e{};
    e.type = MouseEvent::Type::Press;
    
    // Relative position (10, 10) should succeed
    e.position = {10, 10};
    REQUIRE(cb.handle_mouse(e) == true);
    REQUIRE(cb.checked() == true);
    
    // Absolute position (110, 110) should fail
    Checkbox cb2("Option 2");
    cb2.set_rect({100, 150, 200, 30});
    e.position = {110, 160};
    REQUIRE(cb2.handle_mouse(e) == false);
}
