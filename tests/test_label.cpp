#include <catch2/catch_test_macros.hpp>
#include "toolkit/label.hpp"

using namespace toolkit;

TEST_CASE("Label stores text", "[label]") {
    Label l("Hello");
    REQUIRE(l.text() == "Hello");
}

TEST_CASE("Label set_text updates text", "[label]") {
    Label l("Old");
    l.set_text("New");
    REQUIRE(l.text() == "New");
}

TEST_CASE("Label is not focusable", "[label]") {
    Label l("Test");
    REQUIRE(l.focusable() == false);
}

TEST_CASE("Label handle_mouse returns false", "[label]") {
    Label l("Test");
    l.set_rect({0, 0, 100, 30});
    MouseEvent e{};
    e.type = MouseEvent::Type::Press;
    e.position = {50, 15};
    REQUIRE(l.handle_mouse(e) == false);
}

TEST_CASE("Label relative coordinates", "[label]") {
    Label l("Test");
    REQUIRE(l.use_relative_coordinates() == true);
}
