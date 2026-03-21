#include "toolkit/layout.hpp"
#include "toolkit/menubar.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <catch2/catch_test_macros.hpp>
#include <spdlog/spdlog.h>

using namespace toolkit;

TEST_CASE("MenuBar interaction", "[menubar]") {
    DummyPlatformGuard guard;
    Theme::set_current(Theme::create(ThemeStyle::Win11, ColorScheme::Light));

    Window win("Test MenuBar", {800, 600});
    auto root = std::make_unique<VBoxLayout>();
    auto menubar = std::make_unique<MenuBar>();

    bool action1_called = false;
    auto file_menu = menubar->add_menu("File");
    file_menu->add_action("New", [&]() { action1_called = true; });
    file_menu->add_action("Exit", []() {});

    menubar->add_menu("Help")->add_action("About", []() {});

    root->add_widget(std::move(menubar));
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    // Manually paint to trigger layout
    MockPainter painter;
    win.handle_paint(painter);

    // Initially no popup
    REQUIRE(win.has_popup() == false);

    // Click on "File" menu
    MouseEvent me{};
    me.type = MouseEvent::Type::Press;
    me.position = {20, 10};
    spdlog::info("Simulating click at (20, 10)");
    win.handle_mouse(me);

    // Should open "File" menu popup
    REQUIRE(win.has_popup() == true);

    // Click on "New" inside the popup
    // Popup position for the first menu is typically (0, menubar_height)
    // In MockPainter, fm.height = font_size = 14.
    // MenuBar height is fm.height + padding.top + padding.bottom = 14 + 6 + 6 = 26.
    // MenuItem height in Menu is style.font_size + style.item_padding * 2.0f + 4.0f = 14 + 2*4 + 4
    // = 26. Popup starts at (0, 26). Items inside popup start at y=2. So "New" is at y=[26+2,
    // 26+2+26] = [28, 54].
    me.position = {20, 40};
    spdlog::info("Simulating click at (20, 40) for New action");
    win.handle_mouse(me);

    REQUIRE(action1_called == true);
    REQUIRE(win.has_popup() == false);
}
