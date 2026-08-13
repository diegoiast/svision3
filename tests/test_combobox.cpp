#include "svision3/combobox.hpp"
#include "svision3/platform.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/widget_loader.hpp"
#include "svision3/window.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace svision3;

TEST_CASE("Combobox default state", "[combobox]") {
    Combobox cb;
    REQUIRE(cb.selected() == -1);
    REQUIRE(cb.selected_text().empty());
    REQUIRE(cb.cursor() == CursorShape::Hand);
}

TEST_CASE("Combobox interaction with virtual window", "[combobox]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    auto cb_ptr = std::make_unique<Combobox>(std::vector<std::string>{"One", "Two", "Three"});
    auto *cb = cb_ptr.get();
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

    // Fonts on dummy platform are 8x16
    me.position = {20, 20 + 16 * 2}; // Should hit "Two"
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

TEST_CASE("Combobox serialization round-trip", "[combobox][serialization]") {
    Combobox cb({"Red", "Green", "Blue"});
    cb.set_selected(2);

    auto j = cb.to_json();
    REQUIRE(j["type"] == "Combobox");
    REQUIRE(j["items"] == std::vector<std::string>{"Red", "Green", "Blue"});
    REQUIRE(j["current_index"] == 2);

    Combobox cb2;
    cb2.from_json(j);
    REQUIRE(cb2.selected() == 2);
    REQUIRE(cb2.selected_text() == "Blue");
}

TEST_CASE("Combobox serialization no selection", "[combobox][serialization]") {
    Combobox cb({"A", "B"});

    auto j = cb.to_json();
    REQUIRE(j["current_index"] == -1);

    Combobox cb2;
    cb2.from_json(j);
    REQUIRE(cb2.selected() == -1);
    REQUIRE(cb2.selected_text().empty());
}

TEST_CASE("Combobox serialization empty items", "[combobox][serialization]") {
    Combobox cb;
    auto j = cb.to_json();
    REQUIRE(j["items"].empty());
    REQUIRE(j["current_index"] == -1);

    Combobox cb2;
    cb2.from_json(j);
    REQUIRE(cb2.selected() == -1);
}

TEST_CASE("Combobox serialization via WidgetLoader", "[combobox][serialization]") {
    Combobox cb({"X", "Y", "Z"});
    cb.set_selected(1);

    auto j = cb.to_json();
    auto w = WidgetLoader::instance().create_widget(j);
    REQUIRE(w != nullptr);

    auto *restored = dynamic_cast<Combobox *>(w.get());
    REQUIRE(restored != nullptr);
    REQUIRE(restored->selected() == 1);
    REQUIRE(restored->selected_text() == "Y");
}
