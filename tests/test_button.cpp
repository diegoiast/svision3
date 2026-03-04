#include <catch2/catch_test_macros.hpp>
#include "toolkit/button.hpp"

using namespace toolkit;

TEST_CASE("Button is focusable", "[button]") {
    Button b("Click");
    REQUIRE(b.focusable() == true);
}

TEST_CASE("Button mnemonic parsing with &", "[button]") {
    Button b("&Save");
    REQUIRE(b.trigger_mnemonic('s') == true);
    REQUIRE(b.trigger_mnemonic('S') == false);
    REQUIRE(b.trigger_mnemonic('x') == false);
}

TEST_CASE("Button mnemonic in middle", "[button]") {
    Button b("E&xit");
    REQUIRE(b.trigger_mnemonic('x') == true);
    REQUIRE(b.trigger_mnemonic('e') == false);
}

TEST_CASE("Button no mnemonic", "[button]") {
    Button b("Close");
    REQUIRE(b.trigger_mnemonic('c') == false);
    REQUIRE(b.trigger_mnemonic('l') == false);
}

TEST_CASE("Button mnemonic fires on_click", "[button]") {
    Button b("&ok");
    bool clicked = false;
    b.on_click = [&] { clicked = true; };
    b.trigger_mnemonic('o');
    REQUIRE(clicked == true);
}

TEST_CASE("Button mnemonic does not fire when disabled", "[button]") {
    Button b("&ok");
    bool clicked = false;
    b.on_click = [&] { clicked = true; };
    b.set_enabled(false);
    b.trigger_mnemonic('o');
    REQUIRE(clicked == false);
}

TEST_CASE("Button collects mnemonics", "[button]") {
    Button b1("&Save");
    Button b2("Close");
    std::vector<Widget *> out;
    b1.collect_mnemonics(out);
    b2.collect_mnemonics(out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0] == &b1);
}

TEST_CASE("Disabled button cursor is NotAllowed", "[button]") {
    Button b("Test");
    REQUIRE(b.cursor() == CursorShape::Arrow);
    b.set_enabled(false);
    REQUIRE(b.cursor() == CursorShape::NotAllowed);
}

TEST_CASE("Button mouse does nothing when disabled", "[button]") {
    Button b("Test");
    bool clicked = false;
    b.on_click = [&] { clicked = true; };
    b.set_rect({0, 0, 100, 30});
    b.set_enabled(false);

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    REQUIRE(b.handle_mouse(press) == false);

    MouseEvent release{};
    release.type = MouseEvent::Type::Release;
    release.position = {50, 15};
    b.handle_mouse(release);
    REQUIRE(clicked == false);
}

TEST_CASE("Button mouse does nothing when hidden", "[button]") {
    Button b("Test");
    bool clicked = false;
    b.on_click = [&] { clicked = true; };
    b.set_rect({0, 0, 100, 30});
    b.set_visible(false);

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    REQUIRE(b.handle_mouse(press) == false);

    MouseEvent release{};
    release.type = MouseEvent::Type::Release;
    release.position = {50, 15};
    b.handle_mouse(release);
    REQUIRE(clicked == false);
}

TEST_CASE("Button resets state when hidden", "[button]") {
    Button b("Test");
    b.set_rect({0, 0, 100, 30});
    
    MouseEvent move{};
    move.type = MouseEvent::Type::Move;
    move.position = {50, 15};
    b.handle_mouse(move);
    REQUIRE(b.is_hovered() == true);
    
    b.set_visible(false);
    REQUIRE(b.is_hovered() == false);
}
