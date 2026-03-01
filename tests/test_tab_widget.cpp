#include <catch2/catch_test_macros.hpp>
#include "toolkit/tab_widget.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/button.hpp"

using namespace toolkit;

TEST_CASE("TabWidget default state", "[tabwidget]") {
    TabWidget tw;
    REQUIRE(tw.current_index() == 0);
}

TEST_CASE("TabWidget add_tab and current_index", "[tabwidget]") {
    TabWidget tw;
    tw.add_tab("Tab 1", std::make_unique<Label>("Content 1"));
    tw.add_tab("Tab 2", std::make_unique<Label>("Content 2"));
    REQUIRE(tw.current_index() == 0);

    tw.set_current(1);
    REQUIRE(tw.current_index() == 1);
}

TEST_CASE("TabWidget set_current out of range ignored", "[tabwidget]") {
    TabWidget tw;
    tw.add_tab("Tab 1", std::make_unique<Label>("A"));
    tw.set_current(5);
    REQUIRE(tw.current_index() == 0);
    tw.set_current(-1);
    REQUIRE(tw.current_index() == 0);
}

TEST_CASE("TabWidget on_tab_close callback", "[tabwidget]") {
    TabWidget tw;
    tw.add_tab("First", std::make_unique<Label>("A"));
    tw.add_tab("Second", std::make_unique<Label>("B"));

    int closed_idx = -1;
    std::string closed_title;
    tw.on_tab_close = [&](int idx, std::string const &title) {
        closed_idx = idx;
        closed_title = title;
    };
    tw.on_tab_close(1, "Second");
    REQUIRE(closed_idx == 1);
    REQUIRE(closed_title == "Second");
}

TEST_CASE("TabWidget collect_focusables from active tab only", "[tabwidget]") {
    TabWidget tw;

    auto tab1 = std::make_unique<VBoxLayout>();
    tab1->add_widget(std::make_unique<Button>("A"));
    tab1->add_widget(std::make_unique<Button>("B"));

    auto tab2 = std::make_unique<VBoxLayout>();
    tab2->add_widget(std::make_unique<Button>("C"));

    tw.add_tab("Tab 1", std::move(tab1));
    tw.add_tab("Tab 2", std::move(tab2));

    std::vector<Widget *> focusables;
    tw.collect_focusables(focusables);
    REQUIRE(focusables.size() == 2);

    focusables.clear();
    tw.set_current(1);
    tw.collect_focusables(focusables);
    REQUIRE(focusables.size() == 1);
}

TEST_CASE("TabWidget collect_mnemonics from active tab only", "[tabwidget]") {
    TabWidget tw;

    auto tab1 = std::make_unique<VBoxLayout>();
    tab1->add_widget(std::make_unique<Button>("&Save"));

    auto tab2 = std::make_unique<VBoxLayout>();
    tab2->add_widget(std::make_unique<Button>("&Quit"));

    tw.add_tab("Tab 1", std::move(tab1));
    tw.add_tab("Tab 2", std::move(tab2));

    std::vector<Widget *> mnemonics;
    tw.collect_mnemonics(mnemonics);
    REQUIRE(mnemonics.size() == 1);
}
