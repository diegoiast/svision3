#include "toolkit/button.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("Button is focusable", "[button]") {
    Button b("Click");
    REQUIRE(b.is_focusable() == true);
}

TEST_CASE("Button mnemonic parsing with &", "[button]") {
    Button b("&Save");
    REQUIRE(b.trigger_mnemonic("s") == true);
    REQUIRE(b.trigger_mnemonic("S") == false);
    REQUIRE(b.trigger_mnemonic("x") == false);
}

TEST_CASE("Button mnemonic in middle", "[button]") {
    Button b("E&xit");
    REQUIRE(b.trigger_mnemonic("x") == true);
    REQUIRE(b.trigger_mnemonic("e") == false);
}

TEST_CASE("Button no mnemonic", "[button]") {
    Button b("Close");
    REQUIRE(b.trigger_mnemonic("c") == false);
    REQUIRE(b.trigger_mnemonic("l") == false);
}

TEST_CASE("Button mnemonic fires on_click", "[button]") {
    Button b("&ok");
    bool clicked = false;
    b.on_click = [&] { clicked = true; };
    b.trigger_mnemonic("o");
    REQUIRE(clicked == true);
}

TEST_CASE("Button mnemonic does not fire when disabled", "[button]") {
    Button b("&ok");
    bool clicked = false;
    b.on_click = [&] { clicked = true; };
    b.set_enabled(false);
    b.trigger_mnemonic("o");
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
    // Disabled buttons still swallow presses inside their bounds so the event doesn't fall
    // through to a parent's fallback handling (e.g. dragging a window via its title bar).
    REQUIRE(b.handle_mouse(press) == true);

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
    REQUIRE(b.handle_mouse(press) == true);

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

TEST_CASE("Button auto-repeat continues on mouse move inside", "[button]") {
    Button b("Test");
    int click_count = 0;
    b.on_click = [&] { click_count++; };
    b.set_rect({0, 0, 100, 30});
    b.set_auto_repeat(true);

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    REQUIRE(b.handle_mouse(press) == true);
    REQUIRE(b.is_pressed() == true);

    MouseEvent move_inside{};
    move_inside.type = MouseEvent::Type::Move;
    move_inside.position = {60, 15};
    REQUIRE(b.handle_mouse(move_inside) == true);
    REQUIRE(b.is_pressed() == true);
    REQUIRE(click_count == 0);

    MouseEvent move_inside2{};
    move_inside2.type = MouseEvent::Type::Move;
    move_inside2.position = {80, 15};
    REQUIRE(b.handle_mouse(move_inside2) == true);
    REQUIRE(b.is_pressed() == true);
    REQUIRE(click_count == 0);

    MouseEvent release{};
    release.type = MouseEvent::Type::Release;
    release.position = {80, 15};
    b.handle_mouse(release);
    REQUIRE(b.is_pressed() == false);
}

TEST_CASE("Button auto-repeat continues on mouse leave until release", "[button]") {
    Button b("Test");
    int click_count = 0;
    b.on_click = [&] { click_count++; };
    b.set_rect({0, 0, 100, 30});
    b.set_auto_repeat(true);

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    REQUIRE(b.handle_mouse(press) == true);
    REQUIRE(b.is_pressed() == true);

    MouseEvent move_outside{};
    move_outside.type = MouseEvent::Type::Move;
    move_outside.position = {200, 200};
    REQUIRE(b.handle_mouse(move_outside) == false);
    REQUIRE(b.is_pressed() == false);

    MouseEvent release_outside{};
    release_outside.type = MouseEvent::Type::Release;
    release_outside.position = {200, 200};
    b.handle_mouse(release_outside);
    REQUIRE(b.is_pressed() == false);
}

TEST_CASE("Button click from normal state fires on_click", "[button]") {
    Button b("Test");
    int click_count = 0;
    b.on_click = [&] { click_count++; };
    b.set_rect({0, 0, 100, 30});

    REQUIRE(b.is_hovered() == false);
    REQUIRE(b.is_pressed() == false);

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    REQUIRE(b.handle_mouse(press) == true);
    REQUIRE(b.is_pressed() == true);

    MouseEvent release{};
    release.type = MouseEvent::Type::Release;
    release.position = {50, 15};
    b.handle_mouse(release);
    REQUIRE(b.is_pressed() == false);
    REQUIRE(click_count == 1);
}

TEST_CASE("Button release outside does not fire on_click", "[button]") {
    Button b("Test");
    int click_count = 0;
    b.on_click = [&] { click_count++; };
    b.set_rect({0, 0, 100, 30});

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    REQUIRE(b.handle_mouse(press) == true);
    REQUIRE(b.is_pressed() == true);

    MouseEvent move_outside{};
    move_outside.type = MouseEvent::Type::Move;
    move_outside.position = {200, 200};
    b.handle_mouse(move_outside);
    REQUIRE(b.is_pressed() == false);
    REQUIRE(b.is_hovered() == false);

    MouseEvent release_outside{};
    release_outside.type = MouseEvent::Type::Release;
    release_outside.position = {200, 200};
    b.handle_mouse(release_outside);
    REQUIRE(click_count == 0);
    REQUIRE(b.is_pressed() == false);
}

TEST_CASE("Button drag outside then release does not fire on_click", "[button]") {
    Button b("Test");
    int click_count = 0;
    b.on_click = [&] { click_count++; };
    b.set_rect({0, 0, 100, 30});

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    REQUIRE(b.handle_mouse(press) == true);
    REQUIRE(b.is_pressed() == true);

    MouseEvent drag_outside{};
    drag_outside.type = MouseEvent::Type::Drag;
    drag_outside.position = {200, 200};
    b.handle_mouse(drag_outside);
    REQUIRE(b.is_pressed() == false);

    MouseEvent release_outside{};
    release_outside.type = MouseEvent::Type::Release;
    release_outside.position = {200, 200};
    b.handle_mouse(release_outside);
    REQUIRE(click_count == 0);
}

TEST_CASE("Button checkable/checked state", "[button]") {
    Button b("Toggle");
    REQUIRE(b.is_checkable() == false);
    REQUIRE(b.is_checked() == false);

    b.set_checkable(true);
    REQUIRE(b.is_checkable() == true);

    bool toggled = false;
    b.on_toggle = [&](bool c) { toggled = c; };

    b.set_checked(true);
    REQUIRE(b.is_checked() == true);
    REQUIRE(toggled == true);

    b.set_checked(false);
    REQUIRE(b.is_checked() == false);
    REQUIRE(toggled == false);
}

TEST_CASE("Button toggle on click", "[button]") {
    Button b("Toggle");
    b.set_checkable(true);
    b.set_rect({0, 0, 100, 30});

    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.position = {50, 15};
    b.handle_mouse(press);

    MouseEvent release{};
    release.type = MouseEvent::Type::Release;
    release.position = {50, 15};
    b.handle_mouse(release);

    REQUIRE(b.is_checked() == true);

    b.handle_mouse(press);
    b.handle_mouse(release);
    REQUIRE(b.is_checked() == false);
}

TEST_CASE("Button toggle on mnemonic", "[button]") {
    Button b("&Toggle");
    b.set_checkable(true);

    b.trigger_mnemonic("t");
    REQUIRE(b.is_checked() == true);

    b.trigger_mnemonic("t");
    REQUIRE(b.is_checked() == false);
}

TEST_CASE("Button toggle on key", "[button]") {
    Button b("Toggle");
    b.set_checkable(true);

    KeyEvent enter{};
    enter.type = KeyEvent::Type::Press;
    enter.key = Key::Enter;
    b.handle_key(enter);
    REQUIRE(b.is_checked() == true);

    KeyEvent space{};
    space.type = KeyEvent::Type::Press;
    space.text = " ";
    b.handle_key(space);
    REQUIRE(b.is_checked() == false);
}
