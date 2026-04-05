#include <catch2/catch_test_macros.hpp>
#include "toolkit/layout.hpp"
#include "toolkit/button.hpp"
#include "toolkit/window.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include <cstdio>

using namespace toolkit;

TEST_CASE("Layout mouse interaction relative", "[layout]") {
    DummyPlatformGuard guard;
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    
    auto root = std::make_unique<VBoxLayout>();
    root->set_margins({50, 0, 0, 50}); // top=50, left=50
    root->set_spacing(0);
    
    auto inner = std::make_unique<VBoxLayout>();
    inner->set_margins({100, 0, 0, 100}); // top=100, left=100 relative to inner
    
    auto btn_ptr = std::make_unique<Button>("Click me");
    auto* btn = btn_ptr.get();
    bool clicked = false;
    btn->on_click = [&]() { clicked = true; };
    
    auto* inner_ptr = inner.get();
    inner->add_widget(std::move(btn_ptr));
    root->add_widget(std::move(inner));
    win.set_root(std::move(root));
    
    // root is at (0,0).
    // inner is at (50, 50) relative to root.
    // Button is at (100, 100) relative to inner.
    // Physical button position: (50+100, 50+100) = (150, 150).
    
    // Click at absolute (155, 155). This should hit the button.
    MouseEvent me{};
    me.type = MouseEvent::Type::Press;
    me.position = {155, 155};
    win.handle_mouse(me);
    
    // Check if layout was applied correctly
    REQUIRE(inner_ptr->rect().x == 50);
    REQUIRE(btn->rect().x == 100);

    me.type = MouseEvent::Type::Release;
    win.handle_mouse(me);
    
    REQUIRE(clicked == true);
    
    // Click at absolute (105, 105). This should NOT hit the button.
    clicked = false;
    me.type = MouseEvent::Type::Press;
    me.position = {105, 105};
    win.handle_mouse(me);
    
    me.type = MouseEvent::Type::Release;
    win.handle_mouse(me);
    
    REQUIRE(clicked == false);
}
