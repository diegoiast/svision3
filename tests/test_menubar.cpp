#include "svision3/button.hpp"
#include "svision3/theme_base.hpp"
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

TEST_CASE("Menu builders chain", "[menu]") {
    auto theme_menu = std::make_shared<Menu>("Theme");
    theme_menu->add_action("Light", [] {}).add_action("Dark", [] {});

    auto quit = Command::create("Quit", [] {});
    auto menu = Menu{"&File"};

    // Every builder returns the menu, so one expression describes the whole thing.
    auto &returned = menu.add_action("&New", [] {})
                         .add_separator()
                         .add_submenu("Theme", theme_menu)
                         .add_separator()
                         .add_action(quit);

    REQUIRE(&returned == &menu);
    REQUIRE(menu.item_count() == 5);
    REQUIRE(menu.items()[0].command->name() == "&New");
    REQUIRE(menu.items()[1].is_separator());
    REQUIRE(menu.items()[2].is_submenu());
    REQUIRE(menu.items()[2].submenu == theme_menu);
    REQUIRE(menu.items()[3].is_separator());
    REQUIRE(menu.items()[4].command == quit);

    // insert_action and clear chain too.
    auto first = Command::create("First", [] {});
    REQUIRE(&menu.insert_action(0, first) == &menu);
    REQUIRE(menu.items()[0].command == first);
    REQUIRE(menu.clear().item_count() == 0);
}

// Mnemonics belong to Alt. Ctrl and Super are the accelerator namespace, and the
// window used to accept all three -- so a menu whose title started with the same
// letter as a shortcut swallowed it, before the command dispatch ever ran.
TEST_CASE("Ctrl+letter is a shortcut, not a menu mnemonic", "[menubar][keynav]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("mnemonics", {800, 600});

    auto root = std::make_unique<VBoxLayout>();
    auto menubar = std::make_unique<MenuBar>();
    auto *bar = menubar.get();
    menubar->add_menu("&Edit")->add_action("Cut", [] {});
    menubar->add_menu("&View")->add_action("Zoom", [] {});
    root->add_widget(std::move(menubar));
    win.set_root(std::move(root));
    win.handle_resize({800, 600});
    MockPainter painter(new DummyRasterizer);
    win.handle_paint(painter);

    auto fired = 0;
    auto cmd = Command::create("Toggle Docks", [&fired] { ++fired; });
    cmd->set_shortcut("ctrl+e");
    win.add_command(cmd);

    auto press = [&win](char c, bool ctrl, bool alt, bool super) {
        KeyEvent e{};
        e.type = KeyEvent::Type::Press;
        e.ctrl = ctrl;
        e.alt = alt;
        e.super = super;
        e.text = std::string(1, c);
        win.handle_key(e);
    };

    SECTION("Ctrl reaches the command instead of the menu") {
        press('e', true, false, false);
        REQUIRE(fired == 1);
        REQUIRE_FALSE(win.has_popup());
    }

    SECTION("Ctrl+V does not open a View menu") {
        press('v', true, false, false);
        REQUIRE_FALSE(win.has_popup());
    }

    SECTION("Super is an accelerator too") {
        press('e', false, false, true);
        REQUIRE_FALSE(win.has_popup());
    }

    SECTION("Alt still opens the menu") {
        press('e', false, true, false);
        REQUIRE(win.has_popup());
        REQUIRE(bar->find_menu("Edit") >= 0);
        REQUIRE(fired == 0); // the shortcut did not run
    }
}

// Alt+letter is a mnemonic; adding Ctrl or Super makes a separate accelerator.
// Without that distinction a Ctrl+Alt+M command could never fire while a "&Menu"
// button was on screen, because the mnemonic check runs first.
TEST_CASE("Ctrl+Alt+letter is an accelerator, not a mnemonic", "[menubar][keynav]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("chords", {800, 600});

    auto root = std::make_unique<VBoxLayout>();
    auto button = std::make_shared<Button>("&Menu");
    auto clicked = 0;
    button->on_click = [&clicked] { ++clicked; };
    root->add_widget(button);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    auto fired = 0;
    auto cmd = Command::create("Toggle chrome", [&fired] { ++fired; });
    cmd->set_shortcut("ctrl+alt+m");
    win.add_command(cmd);

    auto press = [&win](bool ctrl, bool alt) {
        KeyEvent e{};
        e.type = KeyEvent::Type::Press;
        e.ctrl = ctrl;
        e.alt = alt;
        e.text = "m";
        win.handle_key(e);
    };

    SECTION("Alt+M reaches the mnemonic") {
        press(false, true);
        REQUIRE(clicked == 1);
        REQUIRE(fired == 0);
    }

    SECTION("Ctrl+Alt+M reaches the command") {
        press(true, true);
        REQUIRE(fired == 1);
        REQUIRE(clicked == 0);
    }
}

// A mnemonic should do what a click does. On a button carrying a menu a click
// drops the menu down, but trigger_mnemonic used to call fire_click() -- which on
// such a button fires handlers that are usually unset, so nothing happened.
TEST_CASE("A menu button's mnemonic opens its menu", "[button][keynav]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("menu button", {800, 600});

    auto menu = std::make_shared<Menu>();
    menu->add_action("File", [] {});

    auto root = std::make_unique<VBoxLayout>();
    auto button = std::make_shared<Button>("&Menu");
    button->set_menu(menu);
    root->add_widget(button);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    REQUIRE_FALSE(win.has_popup());

    KeyEvent e{};
    e.type = KeyEvent::Type::Press;
    e.alt = true;
    e.text = "m";
    win.handle_key(e);

    REQUIRE(win.has_popup());
    REQUIRE(menu->is_shown());
}

// Both indents are the menu's, not the item's: one checkable item indents every
// label in that menu, one icon does the same, and a menu with both gets both.
// Otherwise labels stagger, and toggling a mark shifts its own label sideways.
TEST_CASE("Menu columns are reserved menu-wide", "[menu]") {
    using Columns = Theme::MenuColumns;

    struct Probe : BaseTheme {
        mutable std::vector<std::pair<std::string, Columns>> seen;
        void draw_menu_item(Painter &p, Rect const &r, std::string_view text, Icon const &icon,
                            std::string_view shortcut, bool hovered, bool enabled, bool checkable,
                            bool checked, bool show_mnemonics,
                            Columns const &columns) const override {
            seen.emplace_back(std::string(text), columns);
            BaseTheme::draw_menu_item(p, r, text, icon, shortcut, hovered, enabled, checkable,
                                      checked, show_mnemonics, columns);
        }
    };

    auto owned = std::make_unique<Probe>();
    auto *probe = owned.get();
    Theme::set_current(std::move(owned));

    Window win("columns", {800, 600});
    win.handle_resize({800, 600});
    MockPainter painter(new DummyRasterizer);
    win.handle_paint(painter);

    SECTION("a menu with neither reserves nothing") {
        auto menu = std::make_shared<Menu>();
        menu->add_action("Plain", [] {}).add_action("Also plain", [] {});
        menu->show(&win, {10, 10});
        win.handle_paint(painter);

        REQUIRE(probe->seen.size() >= 2);
        for (auto const &[name, columns] : probe->seen) {
            INFO(name);
            REQUIRE(columns.check == 0.0f);
            REQUIRE(columns.icon == 0.0f);
        }
    }

    SECTION("one checkable item indents every label") {
        auto menu = std::make_shared<Menu>();
        auto toggle = Command::create("Toggle", [] {});
        toggle->set_checkable(true);
        menu->add_action("Plain", [] {}).add_action(toggle);
        menu->show(&win, {10, 10});
        win.handle_paint(painter);

        REQUIRE(probe->seen.size() >= 2);
        for (auto const &[name, columns] : probe->seen) {
            INFO(name);
            REQUIRE(columns.check > 0.0f); // including the item that is not checkable
            REQUIRE(columns.icon == 0.0f);
        }
    }
}
