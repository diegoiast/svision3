// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// TabWidget's implicit destructor needs Button complete, for its scroll buttons.
#include "svision3/button.hpp"
#include "svision3/image_widget.hpp"
#include "svision3/label.hpp"
#include "svision3/menu.hpp"
#include "svision3/tab_command_anchor.hpp"
#include "svision3/tab_widget.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/toolbar.hpp"
#include <catch2/catch_test_macros.hpp>
#include <optional>

using namespace svision3;

namespace {

// A widget whose command list is whatever the test says it is, so the anchor can
// be exercised without dragging a TextEdit and its dependencies in.
auto make_widget(std::vector<std::pair<std::string, std::string>> const &commands)
    -> std::shared_ptr<Label> {
    auto w = std::make_shared<Label>("content");
    for (auto const &[name, group] : commands) {
        auto cmd = Command::create(name, [] {});
        cmd->set_group(group);
        w->add_command(cmd);
    }
    return w;
}

auto item_names(Menu const &menu) -> std::vector<std::string> {
    auto names = std::vector<std::string>{};
    for (auto const &item : menu.items()) {
        names.push_back(item.is_separator() ? "---" : item.command->name());
    }
    return names;
}

} // namespace

TEST_CASE("CommandAnchor fills from the active tab", "[commandanchor]") {
    TabWidget tabs;
    tabs.add_tab("text", make_widget({{"Cut", "edit"}, {"Copy", "edit"}}));
    tabs.add_tab("image", make_widget({{"Zoom In", "view"}}));

    Menu edit_menu("&Edit");
    auto anchor = CommandAnchor{tabs, edit_menu, 0, "edit"};

    REQUIRE(item_names(edit_menu) == std::vector<std::string>{"Cut", "Copy"});

    tabs.set_current(1);
    REQUIRE(edit_menu.items().empty());

    tabs.set_current(0);
    REQUIRE(item_names(edit_menu) == std::vector<std::string>{"Cut", "Copy"});
}

TEST_CASE("CommandAnchor filters by group", "[commandanchor]") {
    TabWidget tabs;
    tabs.add_tab("doc", make_widget({{"Save", "file"}, {"Cut", "edit"}, {"Copy", "edit"}}));

    Menu file_menu("&File");
    Menu edit_menu("&Edit");
    Toolbar toolbar;

    auto file_anchor = CommandAnchor{tabs, file_menu, 0, "file"};
    auto edit_anchor = CommandAnchor{tabs, edit_menu, 0, "edit"};
    // An empty group means "no filter at all".
    auto toolbar_anchor = CommandAnchor{tabs, toolbar, 0};

    REQUIRE(item_names(file_menu) == std::vector<std::string>{"Save"});
    REQUIRE(item_names(edit_menu) == std::vector<std::string>{"Cut", "Copy"});
    REQUIRE(toolbar.item_count() == 3);
}

TEST_CASE("CommandAnchor leaves static neighbours alone", "[commandanchor]") {
    TabWidget tabs;
    tabs.add_tab("doc", make_widget({{"Save", "file"}}));
    tabs.add_tab("image", make_widget({{"Zoom In", "view"}}));

    Menu file_menu("&File");
    file_menu.add_action("New", [] {});
    file_menu.add_separator();
    file_menu.add_action("Quit", [] {});

    auto anchor = CommandAnchor{tabs, file_menu, 1, "file"};
    REQUIRE(item_names(file_menu) == std::vector<std::string>{"New", "Save", "---", "Quit"});

    // Switching to a tab with nothing to give must take back exactly the one item
    // the anchor added, and no more.
    tabs.set_current(1);
    REQUIRE(item_names(file_menu) == std::vector<std::string>{"New", "---", "Quit"});
}

TEST_CASE("CommandAnchor separator belongs to the slice", "[commandanchor]") {
    TabWidget tabs;
    tabs.add_tab("doc", make_widget({{"Save", "file"}}));
    tabs.add_tab("image", make_widget({{"Zoom In", "view"}}));

    Menu menu("&File");
    menu.add_action("New", [] {});

    auto before = CommandAnchor{tabs, menu, 1, "file", CommandAnchor::Separator::Before};
    REQUIRE(item_names(menu) == std::vector<std::string>{"New", "---", "Save"});

    // No commands means no separator either: nothing is left dangling.
    tabs.set_current(1);
    REQUIRE(item_names(menu) == std::vector<std::string>{"New"});
}

TEST_CASE("CommandAnchor tracks a closed tab", "[commandanchor]") {
    TabWidget tabs;
    tabs.add_tab("a", make_widget({{"Cut", "edit"}}));
    tabs.add_tab("b", make_widget({{"Zoom In", "view"}}));

    Menu menu("&Edit");
    auto anchor = CommandAnchor{tabs, menu, 0, "edit"};
    REQUIRE(item_names(menu) == std::vector<std::string>{"Cut"});

    // Removing the current tab keeps the index at 0 but puts a different widget
    // behind it, which the anchor still has to notice.
    tabs.remove_tab(0);
    REQUIRE(menu.items().empty());
}

TEST_CASE("TabWidget supports several independent subscribers", "[tabwidget]") {
    TabWidget tabs;
    tabs.add_tab("a", std::make_shared<Label>("A"));
    tabs.add_tab("b", std::make_shared<Label>("B"));

    auto first = 0;
    auto second = 0;
    auto sub_a = tabs.on_current_changed([&first](int) { ++first; });
    {
        auto sub_b = tabs.on_current_changed([&second](int) { ++second; });
        tabs.set_current(1);
        REQUIRE(first == 1);
        REQUIRE(second == 1);
    }

    // sub_b is out of scope now, so only the surviving one fires.
    tabs.set_current(0);
    REQUIRE(first == 2);
    REQUIRE(second == 1);
}

TEST_CASE("TabWidget notifies only on a real change", "[tabwidget]") {
    TabWidget tabs;
    tabs.add_tab("a", std::make_shared<Label>("A"));
    tabs.add_tab("b", std::make_shared<Label>("B"));

    auto calls = 0;
    auto sub = tabs.on_current_changed([&calls](int) { ++calls; });

    tabs.set_current(0); // already current
    REQUIRE(calls == 0);
    tabs.set_current(5); // out of range
    REQUIRE(calls == 0);
    tabs.set_current(1);
    REQUIRE(calls == 1);
}

TEST_CASE("TabWidget Subscription outliving its widget is harmless", "[tabwidget]") {
    auto sub = TabWidget::Subscription{};
    {
        TabWidget tabs;
        tabs.add_tab("a", std::make_shared<Label>("A"));
        sub = tabs.on_current_changed([](int) {});
        REQUIRE(sub.is_active());
    }
    REQUIRE_FALSE(sub.is_active());
    sub.reset(); // must not touch the destroyed TabWidget
}

// The shape demo_dock gives its Help menu: a static About, preceded by whatever
// the active tab has to say about itself. A widget with nothing to add leaves the
// menu as just About, with no stranded separator above it.
TEST_CASE("Help menu shows the active tab's own information command", "[commandanchor]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    TabWidget tabs;
    tabs.add_tab("picture", make_widget({{"Image Information", "help"}}));
    tabs.add_tab("plain", make_widget({{"Cut", "edit"}}));

    Menu help("&Help");
    help.add_action("About", [] {});
    auto anchor = CommandAnchor{tabs, help, 0, "help", CommandAnchor::Separator::After};

    REQUIRE(item_names(help) == std::vector<std::string>{"Image Information", "---", "About"});

    tabs.set_current(1);
    REQUIRE(item_names(help) == std::vector<std::string>{"About"});

    tabs.set_current(0);
    REQUIRE(item_names(help) == std::vector<std::string>{"Image Information", "---", "About"});
}

// ImageWidget contributes only "view" commands. Describing the image is an
// application's job -- what to say and how to show it are presentation decisions
// -- so nothing of the widget's lands in a Help menu.
TEST_CASE("ImageWidget contributes only view commands", "[commandanchor]") {
    TabWidget tabs;
    tabs.add_tab("picture", std::make_shared<ImageWidget>());

    Menu view_menu("&View");
    Menu help_menu("&Help");
    auto view_anchor = CommandAnchor{tabs, view_menu, 0, "view"};
    auto help_anchor = CommandAnchor{tabs, help_menu, 0, "help"};

    REQUIRE(item_names(view_menu) == std::vector<std::string>{"Zoom In", "Zoom Out", "Actual Size",
                                                              "Fit to Window",
                                                              "Transparency Grid"});
    REQUIRE(help_menu.items().empty());
}

// Subscription cannot hold a reference to what it subscribed to: it has an empty
// state, it is movable, and above all it has to survive its Signal and be able to
// tell. These are the three properties a reference member would each break.
TEST_CASE("Subscription is a weak, movable handle", "[subscription]") {
    auto signal = std::optional<Signal<int>>{std::in_place};

    auto calls = 0;
    auto sub = signal->connect([&calls](int) { ++calls; });
    REQUIRE(sub.is_active());

    signal->emit(1);
    REQUIRE(calls == 1);

    SECTION("default-constructed is empty and harmless") {
        auto empty = Subscription{};
        REQUIRE_FALSE(empty.is_active());
        empty.reset(); // must not touch anything
    }

    SECTION("moving transfers the registration exactly once") {
        auto moved = std::move(sub);
        REQUIRE(moved.is_active());
        REQUIRE_FALSE(sub.is_active()); // NOLINT(bugprone-use-after-move)

        signal->emit(2);
        REQUIRE(calls == 2);

        moved.reset();
        signal->emit(3);
        REQUIRE(calls == 2); // disconnected, and only the one registration existed
    }

    SECTION("outliving the signal is safe, and observable") {
        signal.reset();
        REQUIRE_FALSE(sub.is_active());
        sub.reset(); // the case a reference member could not survive
    }

    SECTION("destroying the handle disconnects") {
        {
            auto scoped = signal->connect([&calls](int) { ++calls; });
            signal->emit(4);
            REQUIRE(calls == 3); // both listeners
        }
        signal->emit(5);
        REQUIRE(calls == 4); // only the outer one left
    }
}
