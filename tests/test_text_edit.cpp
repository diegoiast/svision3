#include "toolkit/text_edit.hpp"
#include "toolkit/theme.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("TextEdit relative coordinates", "[textedit]") {
    Theme::set_current(Theme::create(ThemeStyle::MacOS, ColorScheme::Light));
    TextEdit te("Line 1\nLine 2");
    te.set_rect({100, 100, 200, 200});

    MouseEvent e{};
    e.type = MouseEvent::Type::Press;

    // Relative position (10, 10) should succeed (inside bounds and hits text)
    e.position = {10, 10};
    REQUIRE(te.handle_mouse(e) == true);

    // Relative position (10, 190) should fail (inside bounds but hits empty area)
    e.position = {10, 190};
    REQUIRE(te.handle_mouse(e) == false);

    // Absolute position (110, 110) should fail because it's outside local [0, 200]
    e.position = {110, 110};
    REQUIRE(te.handle_mouse(e) == false);
}

TEST_CASE("TextEdit selection and deletion", "[textedit]") {
    TextEdit te("aaa bbb ccc");

    // Select " bbb" (leading space + bbb)
    // "aaa" is 3 chars, so " bbb" starts at col 3 and ends at col 7
    KeyEvent ev{};
    ev.type = KeyEvent::Type::Press;

    // Move to col 3 (after "aaa")
    ev.key = Key::Right;
    for (int i = 0; i < 3; i++) {
        te.handle_key(ev);
    }

    // Select " bbb" (4 characters)
    ev.shift = true;
    for (int i = 0; i < 4; i++) {
        te.handle_key(ev);
    }

    // Press Delete
    ev.key = Key::Delete;
    ev.shift = false;
    te.handle_key(ev);

    REQUIRE(te.text() == "aaa ccc");
}
