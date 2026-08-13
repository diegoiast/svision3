#include "svision3/layout.hpp"
#include "svision3/menubar.hpp"
// #include "svision3/platform/dummy_platform.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/window.hpp"
#include <catch2/catch_test_macros.hpp>
#include <spdlog/spdlog.h>
#include <svision3/text_rasterizer.hpp>

using namespace svision3;

TEST_CASE("MenuBar interaction", "[menubar]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    Window win("Test MenuBar", {800, 600});
    auto root = std::make_unique<VBoxLayout>();
    auto menubar = std::make_unique<MenuBar>();

    int action_called = 0;
    auto file_menu = menubar->add_menu("File");
    file_menu->add_action("New", [&]() { action_called = 10; });
    file_menu->add_action("Exit", [&]() { action_called = 20; });

    menubar->add_menu("Help")->add_action("About", [&]() { action_called = 30; });

    root->add_widget(std::move(menubar));
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    // Manually paint to trigger layout
    MockPainter painter(new DummyRasterizer);
    win.handle_paint(painter);

    // Initially no popup
    REQUIRE(win.has_popup() == false);

    // Font simulation on dummy platform is "8x16" no padding between latters/lines
    // Click on "File" menu, this is hitting the middle of the "l"
    MouseEvent me{};
    me.type = MouseEvent::Type::Press;
    me.position = {18, 10};
    spdlog::info("Simulating click at (18, 10)");
    win.handle_mouse(me);

    // Should open "File" menu popup
    REQUIRE(win.has_popup() == true);

    me.position = {18, 18};
    spdlog::info("Simulating press at (18, 18) for New action");
    win.handle_mouse(me);

    me.type = MouseEvent::Type::Release;
    spdlog::info("Simulating release at (18, 18) for New action");
    win.handle_mouse(me);

    REQUIRE(action_called == 10);
    REQUIRE(win.has_popup() == false);
}
