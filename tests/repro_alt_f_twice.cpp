#include "toolkit/menubar.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/window.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("Alt+F twice should not crash", "[menubar][crash]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    Window win("Test Alt+F Crash", {800, 600});
    auto root = std::make_unique<VBoxLayout>();
    auto menubar_ptr = std::make_unique<MenuBar>();
    menubar_ptr->add_menu("&File")->add_action("Exit", [](){});
    
    root->add_widget(std::move(menubar_ptr));
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    // Manually paint to trigger layout
    MockPainter painter;
    win.handle_paint(painter);

    // 1. First Alt+F
    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;
    ke.alt = true;
    ke.text = "f";
    win.handle_key(ke);
    
    REQUIRE(win.has_popup() == true);

    // 2. Second Alt+F
    win.handle_key(ke);

    // Should not crash
    REQUIRE(win.has_popup() == false);
}
