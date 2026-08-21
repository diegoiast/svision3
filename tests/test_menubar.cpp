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

// Menus that fill from the active context (a CommandAnchor sourcing them from
// the current tab, say) are empty whenever that context has nothing to give, and
// an empty menu is not shown. Sideways navigation used to step by a plain +/-1
// and hand toggle_menu() one of those empties, which it refuses to open -- so the
// arrows died one move away from wherever they started.
namespace {

struct NavFixture {
    Window win{"Test MenuBar nav", {800, 600}};
    MenuBar *bar = nullptr;
    std::shared_ptr<Menu> file, edit, view, window_menu;

    NavFixture() {
        Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
        auto root = std::make_unique<VBoxLayout>();
        auto menubar = std::make_unique<MenuBar>();
        bar = menubar.get();

        file = menubar->add_menu("File");
        file->add_action("New", [] {});
        edit = menubar->add_menu("Edit");
        edit->add_action("Cut", [] {});
        view = menubar->add_menu("View"); // deliberately left empty
        window_menu = menubar->add_menu("Window");
        window_menu->add_action("Tile", [] {});

        root->add_widget(std::move(menubar));
        win.set_root(std::move(root));
        win.handle_resize({800, 600});
        MockPainter painter(new DummyRasterizer);
        win.handle_paint(painter);
    }

    void arrow(Key key) {
        KeyEvent e{};
        e.type = KeyEvent::Type::Press;
        e.key = key;
        win.handle_key(e);
    }
};

} // namespace

TEST_CASE("MenuBar arrows step over empty menus", "[menubar][keynav]") {
    auto f = NavFixture{};

    SECTION("rightwards") {
        f.bar->toggle_menu(0);
        REQUIRE(f.file->is_shown());

        f.arrow(Key::Right);
        REQUIRE(f.edit->is_shown());

        // View is empty, so Right must carry on to Window rather than stalling.
        f.arrow(Key::Right);
        REQUIRE_FALSE(f.view->is_shown());
        REQUIRE(f.window_menu->is_shown());

        f.arrow(Key::Right);
        REQUIRE(f.file->is_shown());
    }

    SECTION("leftwards") {
        f.bar->toggle_menu(0);
        REQUIRE(f.file->is_shown());

        // Left from File wraps past the empty View to Window.
        f.arrow(Key::Left);
        REQUIRE_FALSE(f.view->is_shown());
        REQUIRE(f.window_menu->is_shown());

        f.arrow(Key::Left);
        REQUIRE(f.edit->is_shown());

        f.arrow(Key::Left);
        REQUIRE(f.file->is_shown());
    }

    SECTION("a menu emptied while open is skipped on the next move") {
        f.bar->toggle_menu(1);
        REQUIRE(f.edit->is_shown());

        // What a tab switch does: the Edit slice drains, the View slice fills.
        f.edit->clear();
        f.view->add_action("Zoom In", [] {});

        f.arrow(Key::Right);
        REQUIRE(f.view->is_shown());
    }
}

TEST_CASE("MenuBar arrows keep a lone menu open", "[menubar][keynav]") {
    auto f = NavFixture{};
    f.edit->clear();
    f.window_menu->clear();

    f.bar->toggle_menu(0);
    REQUIRE(f.file->is_shown());

    // File is the only menu with content: there is nowhere to go, and the arrows
    // must not toggle it shut on the way to nowhere.
    f.arrow(Key::Right);
    REQUIRE(f.file->is_shown());
    f.arrow(Key::Left);
    REQUIRE(f.file->is_shown());
}
