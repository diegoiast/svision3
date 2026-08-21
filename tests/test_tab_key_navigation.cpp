// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Ctrl+PageUp/PageDown switches tabs, and TabWidget can only see it if the
// focused content declines it first. Widgets that page on plain PageUp/PageDown
// used to swallow the Ctrl variant too, which killed tab navigation for every
// tab holding one of them.

#include "svision3/button.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/list_view.hpp"
#include "svision3/scrollbar.hpp"
#include "svision3/tab_widget.hpp"
#include "svision3/table_view.hpp"
#include "svision3/text_edit.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/window.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

namespace {

auto page_key(Key key, bool ctrl) -> KeyEvent {
    auto e = KeyEvent{};
    e.type = KeyEvent::Type::Press;
    e.ctrl = ctrl;
    e.key = key;
    return e;
}

auto make_model() -> std::shared_ptr<StringListModel> {
    return std::make_shared<StringListModel>(
        std::vector<std::string>{"one", "two", "three", "four", "five"});
}

// Puts `content` in the first of two tabs, focuses it, and reports whether
// Ctrl+PageDown got through to the TabWidget.
auto switches_tab(std::shared_ptr<Widget> content) -> bool {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));
    Window win("tab navigation", {800, 600});

    auto tabs = std::make_shared<TabWidget>();
    auto *tabs_raw = tabs.get();
    auto *focus_target = content.get();
    tabs->add_tab("first", std::move(content));
    tabs->add_tab("second", std::make_shared<Label>("second"));

    auto root = std::make_unique<VBoxLayout>();
    root->add_widget(std::move(tabs));
    win.set_root(std::move(root));
    win.handle_resize({800, 600});
    win.set_focused_widget(focus_target);

    win.handle_key(page_key(Key::PageDown, true));
    return tabs_raw->get_current_index() == 1;
}

} // namespace

TEST_CASE("Ctrl+PageDown reaches the TabWidget past its content", "[tabwidget][keynav]") {
    REQUIRE(switches_tab(std::make_shared<Label>("plain")));
    REQUIRE(switches_tab(std::make_shared<TextEdit>("one\ntwo\nthree\nfour")));
    REQUIRE(switches_tab(std::make_shared<ListView>(make_model())));
    REQUIRE(switches_tab(std::make_shared<TableView>(make_model())));
    REQUIRE(switches_tab(std::make_shared<Scrollbar>(Orientation::Vertical)));
}

TEST_CASE("TabWidget Ctrl+PageUp/PageDown wrap around", "[tabwidget][keynav]") {
    TabWidget tabs;
    tabs.add_tab("a", std::make_shared<Label>("A"));
    tabs.add_tab("b", std::make_shared<Label>("B"));
    tabs.add_tab("c", std::make_shared<Label>("C"));

    REQUIRE(tabs.handle_key(page_key(Key::PageDown, true)));
    REQUIRE(tabs.get_current_index() == 1);
    tabs.handle_key(page_key(Key::PageDown, true));
    tabs.handle_key(page_key(Key::PageDown, true));
    REQUIRE(tabs.get_current_index() == 0);

    REQUIRE(tabs.handle_key(page_key(Key::PageUp, true)));
    REQUIRE(tabs.get_current_index() == 2);
}

TEST_CASE("Plain PageUp/PageDown still belongs to the content", "[keynav]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    SECTION("TextEdit") {
        auto editor = TextEdit{};
        auto text = std::string{};
        for (auto i = 0; i < 200; ++i) {
            text += "line " + std::to_string(i) + "\n";
        }
        editor.set_text(text);
        editor.set_rect({0, 0, 400, 300});
        editor.set_focused(true);

        REQUIRE(editor.handle_key(page_key(Key::PageDown, false)));
        REQUIRE(editor.handle_key(page_key(Key::PageUp, false)));
        REQUIRE_FALSE(editor.handle_key(page_key(Key::PageDown, true)));
        REQUIRE_FALSE(editor.handle_key(page_key(Key::PageUp, true)));
    }

    SECTION("ListView moves the selection, and only without Ctrl") {
        auto list = ListView{make_model()};
        list.set_rect({0, 0, 200, 40});
        list.set_focused(true);
        list.set_selected(size_t{0});

        REQUIRE(list.handle_key(page_key(Key::PageDown, false)));
        auto selected = list.selected_index();
        REQUIRE(selected.has_value());
        REQUIRE(*selected > 0);

        REQUIRE_FALSE(list.handle_key(page_key(Key::PageDown, true)));
        REQUIRE(list.selected_index() == selected);
    }

    SECTION("Scrollbar pages, and only without Ctrl") {
        auto bar = Scrollbar{Orientation::Vertical};
        bar.set_range(0, 100);
        bar.set_value(50);
        bar.set_focused(true);

        REQUIRE(bar.handle_key(page_key(Key::PageDown, false)));
        auto value = bar.value();
        REQUIRE(value != 50);

        REQUIRE_FALSE(bar.handle_key(page_key(Key::PageDown, true)));
        REQUIRE(bar.value() == value);
    }
}

// Which modifier combinations a paging widget claims, and which it must let
// through. Shift is the widget's own -- it extends the selection rather than
// forming a separate chord -- while Ctrl/Alt/Super build chords that belong to
// the container or the application.
TEST_CASE("Paging widgets claim only bare and shifted page keys", "[keynav]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    struct Mods {
        char const *name;
        bool shift, ctrl, alt, super;
        bool claimed;
    };
    auto const cases = std::array{
        Mods{"bare", false, false, false, false, true},
        Mods{"shift", true, false, false, false, true},
        Mods{"ctrl", false, true, false, false, false},
        Mods{"ctrl+shift", true, true, false, false, false}, // TabWidget reorders tabs
        Mods{"alt", false, false, true, false, false},
        Mods{"super", false, false, false, true, false}, // the macOS accelerator
    };

    auto event = [](Mods const &m, Key key) {
        auto e = KeyEvent{};
        e.type = KeyEvent::Type::Press;
        e.key = key;
        e.shift = m.shift;
        e.ctrl = m.ctrl;
        e.alt = m.alt;
        e.super = m.super;
        return e;
    };

    for (auto const &m : cases) {
        for (auto key : {Key::PageUp, Key::PageDown}) {
            INFO("modifiers: " << m.name);

            auto editor = TextEdit{"a\nb\nc\nd\ne\nf\ng\nh"};
            editor.set_rect({0, 0, 400, 300});
            editor.set_focused(true);
            REQUIRE(editor.handle_key(event(m, key)) == m.claimed);

            auto list = ListView{make_model()};
            list.set_multi_select(true);
            list.set_rect({0, 0, 200, 40});
            list.set_focused(true);
            list.set_selected(size_t{2});
            REQUIRE(list.handle_key(event(m, key)) == m.claimed);

            auto table = TableView{make_model()};
            table.set_rect({0, 0, 200, 40});
            table.set_focused(true);
            REQUIRE(table.handle_key(event(m, key)) == m.claimed);

            auto bar = Scrollbar{Orientation::Vertical};
            bar.set_range(0, 100);
            bar.set_value(50);
            bar.set_focused(true);
            REQUIRE(bar.handle_key(event(m, key)) == m.claimed);
        }
    }
}

TEST_CASE("Shift+PageUp/PageDown still extends a list selection", "[keynav]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    auto list = ListView{make_model()};
    list.set_multi_select(true);
    list.set_rect({0, 0, 200, 40});
    list.set_focused(true);
    list.set_selected(size_t{0});

    auto e = KeyEvent{};
    e.type = KeyEvent::Type::Press;
    e.key = Key::PageDown;
    e.shift = true;
    REQUIRE(list.handle_key(e));
    REQUIRE(list.selection().size() > 1);
}
