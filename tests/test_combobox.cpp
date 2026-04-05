#include "toolkit/combobox.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/window.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("Combobox default state", "[combobox]") {
    Combobox cb;
    REQUIRE(cb.selected() == -1);
    REQUIRE(cb.selected_text().empty());
    REQUIRE(cb.cursor() == CursorShape::Hand);
}

TEST_CASE("Combobox interaction with virtual window", "[combobox]") {
    DummyPlatformGuard guard;
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    auto cb_ptr = std::make_unique<Combobox>(std::vector<std::string>{"One", "Two", "Three"});
    auto* cb = cb_ptr.get();
    cb->set_rect({10, 10, 200, 30});
    win.add_widget(std::move(cb_ptr));

    // Initially closed
    REQUIRE(win.has_popup() == false);
    REQUIRE(cb->selected() == -1);

    // Click to open
    MouseEvent me{};
    me.type = MouseEvent::Type::Press;
    me.position = {20, 20}; // Inside CB (10,10,200,30)
    win.handle_mouse(me);
    REQUIRE(win.has_popup() == true);

    // Dropdown is at (10, 40, 200, height)
    // font_size=14, item_padding=4 -> item_h=22
    // One: [40, 62)
    // Two: [62, 84)
    // Three: [84, 106)
    
    me.position = {20, 73}; // Should hit "Two"
    win.handle_mouse(me);
    
    REQUIRE(cb->selected() == 1);
    REQUIRE(cb->selected_text() == "Two");
    REQUIRE(win.has_popup() == false);

    // Open again with keyboard
    win.set_focused_widget(cb);
    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;
    ke.key = Key::Enter;
    win.handle_key(ke);
    REQUIRE(win.has_popup() == true);

    // Navigate with keys
    ke.key = Key::Down;
    win.handle_key(ke); // Should go to "Three" (idx 2)
    
    // Select with Enter
    ke.key = Key::Enter;
    win.handle_key(ke);
    
    REQUIRE(cb->selected() == 2);
    REQUIRE(cb->selected_text() == "Three");
    REQUIRE(win.has_popup() == false);
}
