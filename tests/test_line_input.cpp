#include "svision3/line_input.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace svision3;

TEST_CASE("LineInput hit tests with read-only and password", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("Placeholder");
    li.set_text("some text");
    li.set_password_mode(true);
    li.set_read_only(true);
    li.set_rect({0, 0, 200, 30});

    // In MacOS theme, padding.right is usually 4. sz is ~16.
    // Clear button position would be 200 - 4 - 16 = 180 to 196.
    // Peek button position shifted would be 180 - 16 - 4 = 160 to 176.

    MouseEvent e{};
    e.type = MouseEvent::Type::Move;

    // Test 1: Hit at 190 (where clear would be, but since it's read-only, peek should move there)
    e.position = {190, 15};
    li.handle_mouse(e);
    // This is expected to FAIL if the bug exists (peek stays at 160)
    CHECK(li.cursor() == CursorShape::Hand);

    // Test 2: Hit at 165 (where peek would be if shifted)
    e.position = {165, 15};
    li.handle_mouse(e);
    // This is expected to FAIL if the bug exists (it will still be Hand at 165)
    CHECK(li.cursor() == CursorShape::IBeam);
}

TEST_CASE("LineInput relative coordinates", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("Placeholder");
    li.set_rect({100, 100, 200, 30});

    MouseEvent e{};
    e.type = MouseEvent::Type::Press;

    // Relative position (10, 10) should succeed
    e.position = {10, 10};
    REQUIRE(li.handle_mouse(e) == true);
}

TEST_CASE("LineInput clear button hover and click", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("Placeholder");
    li.set_text("Some text");
    li.set_rect({0, 0, 200, 30});

    MouseEvent e{};
    e.type = MouseEvent::Type::Move;
    e.position = {190, 15};

    REQUIRE(li.handle_mouse(e) == true);
    REQUIRE(li.cursor() == CursorShape::Hand);

    e.type = MouseEvent::Type::Press;
    REQUIRE(li.handle_mouse(e) == true);

    e.type = MouseEvent::Type::Release;
    REQUIRE(li.handle_mouse(e) == true);

    REQUIRE(li.text().empty());
}

TEST_CASE("Serialize/de-serialize", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("placeholder");
    li.set_text("Some text");

    {
        li.set_password_mode(true);
        li.set_read_only(false);
        auto j = li.to_json();
        REQUIRE(j["text"] == "Some text");
        REQUIRE(j["placeholder"] == "placeholder");
        REQUIRE(j["read_only"] == false);
        REQUIRE(j["is_password"] == true);
    }
    {
        li.set_password_mode(false);
        li.set_read_only(true);
        auto j = li.to_json();
        REQUIRE(j["read_only"] == true);
        REQUIRE(j["is_password"] == false);
    }
}