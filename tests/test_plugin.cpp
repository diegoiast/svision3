// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/button.hpp"
#include "svision3/command_source.hpp"
#include "svision3/dock_area.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/menubar.hpp"
#include "svision3/plugin.hpp"
#include "svision3/tab_command_anchor.hpp"
#include "svision3/text_edit.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/window.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace svision3;

namespace {

class TestHost : public PluginHost {
  public:
    TestHost(Window *win, DockArea *docks, MenuBar *bar, CommandSource *commands)
        : win_(win), docks_(docks), bar_(bar), commands_(commands) {}

    Window *window() const override { return win_; }
    DockArea *docks() const override { return docks_; }
    MenuBar *menubar() const override { return bar_; }
    CommandSource *commands() const override { return commands_; }

  private:
    Window *win_ = nullptr;
    DockArea *docks_ = nullptr;
    MenuBar *bar_ = nullptr;
    CommandSource *commands_ = nullptr;
};

// The shape demo_dock's ProjectPlugin has: some docks, a top-level menu, and an
// anchor writing into that menu.
class ProjectishPlugin : public Plugin {
  public:
    std::string_view name() const override { return "project"; }

    void install(PluginHost &host) override {
        if (auto *docks = host.docks()) {
            docks_ = {
                docks->add_dock(DockPosition::East, "Outline", std::make_shared<Label>("outline")),
                docks->add_dock(DockPosition::South, "Console", std::make_shared<TextEdit>("log")),
            };
        }
        auto *bar = host.menubar();
        if (!bar) {
            return;
        }
        menu_ = std::make_shared<Menu>("&Project");
        menu_->add_action("Build", [] {});
        menu_->add_action("Clean", [] {});
        menu_->add_action("Run", [] {});

        auto help = bar->find_menu("Help");
        bar->insert_menu(help < 0 ? bar->menu_count() : help, menu_);

        if (auto *commands = host.commands()) {
            anchor_ = std::make_unique<CommandAnchor>(*commands, *menu_, 0, "project",
                                                      CommandAnchor::Separator::After);
        }
    }

    void uninstall(PluginHost &host) override {
        anchor_.reset();
        if (auto *bar = host.menubar()) {
            bar->remove_menu(menu_);
        }
        menu_.reset();
        if (auto *docks = host.docks()) {
            for (auto const &panel : docks_) {
                docks->remove_dock(panel);
            }
        }
        docks_.clear();
    }

  private:
    std::vector<std::weak_ptr<Widget>> docks_;
    std::shared_ptr<Menu> menu_;
    std::unique_ptr<CommandAnchor> anchor_;
};

struct Fixture {
    Window win{"plugins", {800, 600}};
    DockArea *docks = nullptr;
    MenuBar *bar = nullptr;
    std::unique_ptr<FocusCommandSource> source;
    std::unique_ptr<TestHost> host;
    std::unique_ptr<PluginManager> plugins;

    Fixture() {
        Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

        auto dock_area = std::make_shared<DockArea>();
        docks = dock_area.get();
        dock_area->set_center(std::make_shared<TextEdit>("centre"));

        auto menubar = std::make_shared<MenuBar>();
        bar = menubar.get();
        menubar->add_menu("File")->add_action("New", [] {});
        menubar->add_menu("Help")->add_action("About", [] {});

        auto root = std::make_unique<VBoxLayout>();
        root->add_widget(menubar);
        root->add_widget(dock_area, 1);
        win.set_root(std::move(root));
        win.handle_resize({800, 600});

        source = std::make_unique<FocusCommandSource>(win);
        host = std::make_unique<TestHost>(&win, docks, bar, source.get());
        plugins = std::make_unique<PluginManager>(*host);
    }

    int dock_count(DockPosition pos) const {
        auto tab = docks->dock_tab_widget(pos).lock();
        return tab ? static_cast<int>(tab->get_tab_count()) : -1;
    }
};

} // namespace

TEST_CASE("A plugin merges its menu and docks in", "[plugin]") {
    auto f = Fixture{};

    REQUIRE(f.bar->menu_count() == 2);
    REQUIRE(f.dock_count(DockPosition::East) == 0);
    REQUIRE(f.dock_count(DockPosition::South) == 0);

    REQUIRE(f.plugins->install(std::make_shared<ProjectishPlugin>()));
    REQUIRE(f.plugins->is_installed("project"));

    REQUIRE(f.bar->menu_count() == 3);
    // Merged ahead of Help, which stays last.
    REQUIRE(f.bar->menu_at(1)->display_title() == "Project");
    REQUIRE(f.bar->menu_at(2)->display_title() == "Help");
    REQUIRE(f.dock_count(DockPosition::East) == 1);
    REQUIRE(f.dock_count(DockPosition::South) == 1);
}

TEST_CASE("A plugin unmerges everything it added", "[plugin]") {
    auto f = Fixture{};
    f.plugins->install(std::make_shared<ProjectishPlugin>());

    REQUIRE(f.plugins->remove("project"));
    REQUIRE_FALSE(f.plugins->is_installed("project"));

    // Nothing of the plugin's may still be reachable: its shared library could be
    // unloaded the moment after this returns.
    REQUIRE(f.bar->menu_count() == 2);
    REQUIRE(f.bar->find_menu("Project") == -1);
    REQUIRE(f.dock_count(DockPosition::East) == 0);
    REQUIRE(f.dock_count(DockPosition::South) == 0);

    // The window must not be left holding pointers into the removed panels.
    REQUIRE(f.bar->menu_at(0)->display_title() == "File");
    REQUIRE(f.bar->menu_at(1)->display_title() == "Help");
}

TEST_CASE("A plugin can be switched off and on again", "[plugin]") {
    auto f = Fixture{};
    // Registered once. Everything after this toggles the same instance, which is
    // what install()/uninstall() being re-enterable buys.
    REQUIRE(f.plugins->register_plugin(std::make_shared<ProjectishPlugin>()));

    for (auto round = 0; round < 3; ++round) {
        INFO("round " << round);
        REQUIRE(f.plugins->install("project"));
        REQUIRE(f.bar->menu_count() == 3);
        REQUIRE(f.dock_count(DockPosition::South) == 1);
        REQUIRE(f.dock_count(DockPosition::East) == 1);

        REQUIRE(f.plugins->remove("project"));
        REQUIRE(f.bar->menu_count() == 2);
        REQUIRE(f.dock_count(DockPosition::South) == 0);
        REQUIRE(f.dock_count(DockPosition::East) == 0);
    }
}

TEST_CASE("PluginManager refuses a duplicate name", "[plugin]") {
    auto f = Fixture{};

    REQUIRE(f.plugins->install(std::make_shared<ProjectishPlugin>()));
    REQUIRE_FALSE(f.plugins->install(std::make_shared<ProjectishPlugin>()));
    REQUIRE(f.plugins->installed_names() == std::vector<std::string>{"project"});
    REQUIRE(f.plugins->registered_names() == std::vector<std::string>{"project"});
    REQUIRE(f.bar->menu_count() == 3);
}

TEST_CASE("PluginManager uninstalls on destruction", "[plugin]") {
    auto f = Fixture{};
    f.plugins->install(std::make_shared<ProjectishPlugin>());
    REQUIRE(f.bar->menu_count() == 3);

    f.plugins.reset();
    REQUIRE(f.bar->menu_count() == 2);
    REQUIRE(f.dock_count(DockPosition::East) == 0);
}

TEST_CASE("Removing a plugin unsubscribes its anchor", "[plugin]") {
    auto f = Fixture{};
    f.plugins->install(std::make_shared<ProjectishPlugin>());
    f.plugins->remove("project");

    // The anchor subscribed to the shared CommandSource. If removal left that
    // subscription behind, this focus change would call into a destroyed anchor
    // and write into a destroyed menu.
    auto document = std::make_shared<TextEdit>("another");
    f.win.set_focused_widget(document.get());
    REQUIRE(f.source->current_context() == document.get());
}

// Registration is separate from installation because a plugin cannot own the
// switch that turns it back on: everything install() added is gone after
// uninstall(), so an enable item living there would take itself away.
TEST_CASE("The registry outlives install and uninstall", "[plugin]") {
    auto f = Fixture{};
    f.plugins->register_plugin(std::make_shared<ProjectishPlugin>());

    REQUIRE(f.plugins->is_registered("project"));
    REQUIRE_FALSE(f.plugins->is_installed("project"));

    // The very same instance is switched on, off, and on again -- no rebuilding,
    // so no factory is needed to describe how to rebuild it.
    auto instance = f.plugins->find("project");
    REQUIRE(instance != nullptr);

    REQUIRE(f.plugins->install("project"));
    REQUIRE(f.plugins->is_installed("project"));
    REQUIRE(f.plugins->remove("project"));

    REQUIRE(f.plugins->is_registered("project"));
    REQUIRE(f.plugins->registered_names() == std::vector<std::string>{"project"});
    REQUIRE(f.plugins->install("project"));
    REQUIRE(f.plugins->find("project") == instance);
}

TEST_CASE("install() refuses an unregistered plugin", "[plugin]") {
    auto f = Fixture{};
    REQUIRE_FALSE(f.plugins->install("nope"));
    REQUIRE_FALSE(f.plugins->is_installed("nope"));
}

TEST_CASE("The plugin menu toggles and tracks state", "[plugin]") {
    auto f = Fixture{};
    f.plugins->register_plugin(std::make_shared<ProjectishPlugin>());

    auto menu = f.plugins->make_menu();
    REQUIRE(menu->item_count() == 1);

    auto cmd = menu->items()[0].command;
    REQUIRE(cmd->is_checkable());
    REQUIRE_FALSE(cmd->is_checked());

    cmd->execute();
    REQUIRE(f.plugins->is_installed("project"));
    REQUIRE(cmd->is_checked());
    REQUIRE(f.bar->find_menu("Project") >= 0);

    cmd->execute();
    REQUIRE_FALSE(f.plugins->is_installed("project"));
    REQUIRE_FALSE(cmd->is_checked());
    REQUIRE(f.bar->find_menu("Project") == -1);

    // The item survives its plugin being uninstalled -- the whole point.
    cmd->execute();
    REQUIRE(f.plugins->is_installed("project"));
}

TEST_CASE("The plugin menu goes inert if the manager dies first", "[plugin]") {
    auto f = Fixture{};
    f.plugins->register_plugin(std::make_shared<ProjectishPlugin>());

    auto menu = f.plugins->make_menu();
    auto cmd = menu->items()[0].command;

    f.plugins.reset();
    cmd->execute(); // must not touch the destroyed manager
    SUCCEED("menu item survived its manager");
}

TEST_CASE("unregister uninstalls and releases the instance", "[plugin]") {
    auto f = Fixture{};
    auto plugin = std::make_shared<ProjectishPlugin>();
    f.plugins->install(plugin);
    REQUIRE(f.bar->menu_count() == 3);

    // The caller's shared_ptr is the only other reference, so use_count tells us
    // whether the manager let go.
    REQUIRE(plugin.use_count() == 2);

    REQUIRE(f.plugins->unregister("project"));
    REQUIRE_FALSE(f.plugins->is_registered("project"));
    REQUIRE_FALSE(f.plugins->is_installed("project"));
    REQUIRE(f.bar->menu_count() == 2); // uninstalled on the way out
    REQUIRE(plugin.use_count() == 1);  // and released

    REQUIRE_FALSE(f.plugins->unregister("project"));
}

TEST_CASE("remove_all leaves the registry intact", "[plugin]") {
    auto f = Fixture{};
    f.plugins->install(std::make_shared<ProjectishPlugin>());

    f.plugins->remove_all();
    REQUIRE_FALSE(f.plugins->is_installed("project"));
    REQUIRE(f.plugins->is_registered("project"));
    REQUIRE(f.bar->menu_count() == 2);

    // Still switchable back on afterwards.
    REQUIRE(f.plugins->install("project"));
    REQUIRE(f.bar->menu_count() == 3);
}

TEST_CASE("Two plugin menus stay in step", "[plugin]") {
    auto f = Fixture{};
    f.plugins->register_plugin(std::make_shared<ProjectishPlugin>());

    // A menubar menu and a button's drop-down cannot share one Menu instance --
    // each carries its own open/hover state -- so an application showing the list
    // in two places has two copies to keep honest.
    auto first = f.plugins->make_menu();
    auto second = f.plugins->make_menu();

    first->items()[0].command->execute();
    REQUIRE(f.plugins->is_installed("project"));
    REQUIRE(first->items()[0].command->is_checked());

    // Stale until opened, then correct: on_about_to_show re-reads the state.
    REQUIRE_FALSE(second->items()[0].command->is_checked());
    REQUIRE(second->on_about_to_show);
    second->on_about_to_show();
    REQUIRE(second->items()[0].command->is_checked());

    // And the same the other way round.
    second->items()[0].command->execute();
    REQUIRE_FALSE(f.plugins->is_installed("project"));
    first->on_about_to_show();
    REQUIRE_FALSE(first->items()[0].command->is_checked());
}

// remove_dock takes the handle add_dock returned, not a title. A title identifies
// nothing: two panels can share one, and a component removing "Outline" by name
// would happily take out an Outline that somebody else put there.
TEST_CASE("remove_dock removes exactly the panel it was given", "[dockarea]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    auto docks = DockArea{};
    auto mine = std::make_shared<Label>("mine");
    auto theirs = std::make_shared<Label>("theirs");

    // Same position, same title -- exactly the case a name cannot tell apart.
    auto mine_ref = docks.add_dock(DockPosition::East, "Outline", mine);
    auto theirs_ref = docks.add_dock(DockPosition::East, "Outline", theirs);

    auto tab = docks.dock_tab_widget(DockPosition::East).lock();
    REQUIRE(tab);
    REQUIRE(tab->get_tab_count() == 2);

    REQUIRE(docks.remove_dock(theirs_ref));
    REQUIRE(tab->get_tab_count() == 1);
    REQUIRE(tab->get_widget(0) == mine.get()); // the other one is untouched

    // Removing something that is not docked here, or is already gone, is a no-op.
    REQUIRE_FALSE(docks.remove_dock(theirs_ref));
    REQUIRE_FALSE(docks.remove_dock(std::weak_ptr<Widget>{}));
    REQUIRE_FALSE(docks.remove_dock(std::make_shared<Label>("stranger")));
    REQUIRE(tab->get_tab_count() == 1);
}

TEST_CASE("remove_dock finds the panel without being told where", "[dockarea]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    auto docks = DockArea{};
    auto west = docks.add_dock(DockPosition::West, "Inspector", std::make_shared<Label>("w"));
    auto south = docks.add_dock(DockPosition::South, "Console", std::make_shared<Label>("s"));

    REQUIRE(docks.remove_dock(south));
    REQUIRE(docks.dock_tab_widget(DockPosition::South).lock()->get_tab_count() == 0);
    REQUIRE(docks.dock_tab_widget(DockPosition::West).lock()->get_tab_count() == 1);

    REQUIRE(docks.remove_dock(west));
    REQUIRE(docks.dock_tab_widget(DockPosition::West).lock()->get_tab_count() == 0);
}

// remove_menu takes the handle, for the same reason remove_dock does: a title is
// not an identity. Removing "Help" by name would take out whichever came first,
// possibly one another component contributed.
TEST_CASE("remove_menu removes exactly the menu it was given", "[menubar]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    auto bar = MenuBar{};
    auto mine = bar.add_menu("Tools");
    mine->add_action("Mine", [] {});
    auto theirs = bar.add_menu("Tools"); // same title, different menu
    theirs->add_action("Theirs", [] {});

    REQUIRE(bar.menu_count() == 2);
    REQUIRE(bar.remove_menu(theirs));
    REQUIRE(bar.menu_count() == 1);
    REQUIRE(bar.menu_at(0) == mine);

    // Already gone, never there, and null are all no-ops.
    REQUIRE_FALSE(bar.remove_menu(theirs));
    REQUIRE_FALSE(bar.remove_menu(std::make_shared<Menu>("Stranger")));
    REQUIRE_FALSE(bar.remove_menu(nullptr));
    REQUIRE(bar.menu_count() == 1);
}

TEST_CASE("Removing by name is a composition, not an overload", "[menubar]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    auto bar = MenuBar{};
    bar.add_menu("File")->add_action("New", [] {});
    bar.add_menu("Help")->add_action("About", [] {});

    REQUIRE(bar.remove_menu(bar.menu_at(bar.find_menu("Help"))));
    REQUIRE(bar.menu_count() == 1);
    REQUIRE(bar.find_menu("Help") == -1);

    // find_menu returns -1 and menu_at returns null, which remove_menu declines.
    REQUIRE_FALSE(bar.remove_menu(bar.menu_at(bar.find_menu("Help"))));
}
