// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/button.hpp"
#include "svision3/combobox.hpp"
#include "svision3/command_source.hpp"
#include "svision3/image_widget.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/line_input.hpp"
#include "svision3/menu.hpp"
#include "svision3/tab_command_anchor.hpp"
#include "svision3/tab_widget.hpp"
#include "svision3/text_edit.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/toolbar.hpp"
#include "svision3/window.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

namespace {

auto item_names(Menu const &menu) -> std::vector<std::string> {
    auto names = std::vector<std::string>{};
    for (auto const &item : menu.items()) {
        names.push_back(item.is_separator() ? "---" : item.command->name());
    }
    return names;
}

} // namespace

TEST_CASE("FocusCommandSource follows focus into any pane", "[commandsource]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("context", {800, 600});

    // A centre document and a "build output" pane, the way a dock lays them out.
    auto document = std::make_shared<TextEdit>("centre");
    auto console = std::make_shared<TextEdit>("build output");

    auto root = std::make_unique<VBoxLayout>();
    root->add_widget(document);
    root->add_widget(console);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    auto source = FocusCommandSource{win};
    REQUIRE(source.current_context() == nullptr);

    win.set_focused_widget(document.get());
    REQUIRE(source.current_context() == document.get());

    // The point of moving this to the window: a dock pane is a context too.
    win.set_focused_widget(console.get());
    REQUIRE(source.current_context() == console.get());
}

TEST_CASE("FocusCommandSource ignores chrome", "[commandsource]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("context", {800, 600});

    auto document = std::make_shared<TextEdit>("centre");
    auto toolbar = std::make_shared<Toolbar>();
    auto combo = std::make_shared<Combobox>(std::vector<std::string>{"one", "two"});
    // A LineInput registers cut/copy/paste of its own, so it is the dangerous
    // case: without the opt-in flag it would quietly capture the Edit menu.
    auto search = std::make_shared<LineInput>();
    toolbar->add_widget(combo);
    toolbar->add_widget(search);

    auto root = std::make_unique<VBoxLayout>();
    root->add_widget(document);
    root->add_widget(toolbar);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    auto source = FocusCommandSource{win};
    win.set_focused_widget(document.get());
    REQUIRE(source.current_context() == document.get());

    win.set_focused_widget(combo.get());
    REQUIRE(source.current_context() == document.get());

    win.set_focused_widget(search.get());
    REQUIRE(source.current_context() == document.get());

    // Losing focus entirely must not blank the menus either.
    win.set_focused_widget(nullptr);
    REQUIRE(source.current_context() == document.get());
}

TEST_CASE("FocusCommandSource walks up to the nearest context", "[commandsource]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("context", {800, 600});

    // Focus normally lands on a child of the document, not the document itself.
    auto document = std::make_shared<ImageWidget>();
    auto inner = std::make_shared<Button>("nested");
    inner->set_parent(document.get());

    auto root = std::make_unique<VBoxLayout>();
    root->add_widget(document);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    auto source = FocusCommandSource{win};
    win.set_focused_widget(inner.get());
    REQUIRE(source.current_context() == document.get());
}

TEST_CASE("FocusCommandSource drops a context that is destroyed", "[commandsource]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("context", {800, 600});

    auto tabs = std::make_shared<TabWidget>();
    auto document = std::make_shared<TextEdit>("doc");
    tabs->add_tab("doc", document);
    tabs->add_tab("other", std::make_shared<Label>("plain"));

    auto root = std::make_unique<VBoxLayout>();
    root->add_widget(tabs);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    auto source = FocusCommandSource{win};
    win.set_focused_widget(document.get());
    REQUIRE(source.current_context() == document.get());

    // remove_tab() destroys the widget after telling the window to forget it. The
    // "keep the last context" rule must not keep a dangling one.
    document.reset();
    tabs->remove_tab(0);
    REQUIRE(source.current_context() == nullptr);
}

TEST_CASE("Menus follow the focused pane end to end", "[commandsource]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("context", {800, 600});

    auto document = std::make_shared<TextEdit>("centre");
    auto picture = std::make_shared<ImageWidget>();
    auto combo = std::make_shared<Combobox>(std::vector<std::string>{"one", "two"});

    auto root = std::make_unique<VBoxLayout>();
    root->add_widget(document);
    root->add_widget(picture);
    root->add_widget(combo);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    auto source = FocusCommandSource{win};
    Menu edit_menu("&Edit");
    Menu view_menu("&View");
    auto edit_anchor = CommandAnchor{source, edit_menu, 0, "edit"};
    auto view_anchor = CommandAnchor{source, view_menu, 0, "view"};

    win.set_focused_widget(document.get());
    REQUIRE(item_names(edit_menu) ==
            std::vector<std::string>{"Undo", "Redo", "Cut", "Copy", "Paste", "Select All"});
    REQUIRE(view_menu.items().empty());

    win.set_focused_widget(picture.get());
    REQUIRE(edit_menu.items().empty());
    REQUIRE(item_names(view_menu) == std::vector<std::string>{"Zoom In", "Zoom Out", "Actual Size",
                                                              "Fit to Window",
                                                              "Transparency Grid"});

    // Reaching for the toolbar leaves both menus exactly as they were.
    win.set_focused_widget(combo.get());
    REQUIRE(edit_menu.items().empty());
    REQUIRE(item_names(view_menu).size() == 5);
}

TEST_CASE("set_command_context opts a widget in or out", "[commandsource]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("context", {800, 600});

    auto document = std::make_shared<TextEdit>("centre");
    auto search = std::make_shared<LineInput>();
    // An application that *wants* Edit to act on its search field says so.
    search->set_command_context(true);

    auto root = std::make_unique<VBoxLayout>();
    root->add_widget(document);
    root->add_widget(search);
    win.set_root(std::move(root));
    win.handle_resize({800, 600});

    auto source = FocusCommandSource{win};
    win.set_focused_widget(document.get());
    REQUIRE(source.current_context() == document.get());

    win.set_focused_widget(search.get());
    REQUIRE(source.current_context() == search.get());

    // And a document can opt out, staying focusable without driving the menus.
    document->set_command_context(false);
    win.set_focused_widget(document.get());
    REQUIRE(source.current_context() == search.get());
}
