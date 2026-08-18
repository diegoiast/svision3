#include "svision3/context_menu.hpp"
#include "svision3/menu.hpp"
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

TEST_CASE("ContextMenu clamps to the client area, not the CSD shadow", "[contextmenu]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    win.set_csd_mode(true);
    win.relayout();

    auto content = win.content_rect();
    // A CSD theme with a shadow gives a non-trivial inset; if it were ever zero this test
    // would pass vacuously without exercising the clamp this guards against.
    REQUIRE(content.x > 0.0f);

    std::vector<MenuItem> items;
    items.push_back(MenuItem::action("Action 1", [] {}));
    items.push_back(MenuItem::action("Action 2", [] {}));

    {
        // Right edge: past the client area on x, so the right-edge clamp engages.
        ContextMenu menu(items);
        menu.show(&win, {content.x + content.width + 100.0f, content.y + 10.0f});
        auto bounds = menu.bounds();
        // Regression for a bug where the clamp used Window::size() (which includes the CSD
        // shadow/border) instead of the client area, letting the menu land out past the
        // window's actual visible edge, offset by the shadow size.
        REQUIRE(bounds.x + bounds.width <= content.x + content.width + 0.01f);
    }
    {
        // Top edge: above the client area, so the top-edge clamp engages.
        ContextMenu menu(items);
        menu.show(&win, {content.x + 10.0f, content.y - 100.0f});
        auto bounds = menu.bounds();
        REQUIRE(bounds.y >= content.y - 0.01f);
    }
}

TEST_CASE("Menu clamps to the client area, not the CSD shadow", "[menu]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    win.set_csd_mode(true);
    win.relayout();

    auto content = win.content_rect();
    REQUIRE(content.x > 0.0f);

    auto menu = std::make_shared<Menu>("Test Menu");
    menu->add_action("Action 1", [] {});
    menu->add_action("Action 2", [] {});

    // Positioned past the window's client area on both axes, so both the right and bottom
    // clamps engage.
    menu->show(&win, {content.x + content.width + 100.0f, content.y + content.height + 100.0f});

    auto bounds = menu->bounds();
    // Same regression as ContextMenu above: clamping against Window::size() instead of the
    // client area would let the menu land out past the window's actual visible edge.
    REQUIRE(bounds.x + bounds.width <= content.x + content.width + 0.01f);
    REQUIRE(bounds.y + bounds.height <= content.y + content.height + 0.01f);
    REQUIRE(bounds.x >= content.x - 0.01f);
    REQUIRE(bounds.y >= content.y - 0.01f);
}
