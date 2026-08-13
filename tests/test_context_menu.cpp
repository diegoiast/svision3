#include "svision3/context_menu.hpp"
#include "svision3/platform.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/window.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

TEST_CASE("ContextMenu interaction", "[contextmenu]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});

    bool action1_called = false;
    bool action2_called = false;

    std::vector<MenuItem> items;
    items.push_back(MenuItem::action("Action 1", [&]() { action1_called = true; }));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action("Action 2", [&]() { action2_called = true; }));

    ContextMenu menu(std::move(items));

    // Show at (100, 100)
    menu.show(&win, {100, 100});
    REQUIRE(win.has_popup() == true);

    MouseEvent me{};
    me.type = MouseEvent::Type::Press;

    // Click Action 2
    // Popup is at (100, 100). We need to click at (3 letters on, 2 line down, middle of line)
    me.position = {100 + 3 * 8, 100 + 8 * 2 + 4};
    win.handle_mouse(me);

    me.type = MouseEvent::Type::Release;
    // me.position = {100 + 3 * 8, 100 + 8};
    win.handle_mouse(me);

    REQUIRE(win.has_popup() == false); // Closed after click
    REQUIRE(action2_called == true);
    REQUIRE(action1_called == false);

    // Show again and test keyboard
    menu.show(&win, {100, 100});
    REQUIRE(win.has_popup() == true);

    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;

    // Select first item (it's not hovered by default usually, but handle_key next_enabled might set
    // it)
    ke.key = Key::Down;
    win.handle_key(ke);

    ke.key = Key::Enter;
    win.handle_key(ke);

    REQUIRE(action1_called == true);
    REQUIRE(win.has_popup() == false);
}
