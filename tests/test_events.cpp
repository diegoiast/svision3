#include "svision3/events.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

TEST_CASE("MouseEvent defaults", "[events]") {
    MouseEvent e{};
    REQUIRE(e.button == 0);
    REQUIRE(e.click_count == 1);
    REQUIRE(e.shift == false);
    REQUIRE(e.ctrl == false);
    REQUIRE(e.super == false);
    REQUIRE(e.scroll_dx == 0.0f);
    REQUIRE(e.scroll_dy == 0.0f);
}

TEST_CASE("KeyEvent defaults", "[events]") {
    KeyEvent e{};
    REQUIRE(e.type == KeyEvent::Type::Press);
    REQUIRE(e.key == Key::NoKey);
    REQUIRE(e.text.empty());
    REQUIRE(e.shift == false);
    REQUIRE(e.ctrl == false);
    REQUIRE(e.alt == false);
    REQUIRE(e.super == false);
}

TEST_CASE("Key enum values are distinct", "[events]") {
    REQUIRE(Key::NoKey != Key::Backspace);
    REQUIRE(Key::Left != Key::Right);
    REQUIRE(Key::Home != Key::End);
    REQUIRE(Key::Tab != Key::Enter);
}
