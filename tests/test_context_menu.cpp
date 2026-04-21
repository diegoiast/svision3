#include "toolkit/context_menu.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/window.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

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

    // Test mouse selection
    // item_h = 14 + 4*2 + 4 = 26
    // Action 1: y=[0, 26) relative to popup origin
    // Sep: y=[26, 33) (sep_h = 7)
    // Action 2: y=[33, 59)

    MouseEvent me{};
    me.type = MouseEvent::Type::Press;

    // Click Action 2
    // Popup is at (100, 100). We need to click at (100+some_x, 100+40)
    me.position = {150, 145};
    win.handle_mouse(me);

    me.type = MouseEvent::Type::Release;
    me.position = {150, 145};
    win.handle_mouse(me);

    REQUIRE(action2_called == true);
    REQUIRE(action1_called == false);
    REQUIRE(win.has_popup() == false); // Closed after click

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
