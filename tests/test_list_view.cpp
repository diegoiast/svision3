#include <catch2/catch_test_macros.hpp>
#include "toolkit/list_view.hpp"

using namespace toolkit;

static std::shared_ptr<StringListAdapter> make_adapter(int n = 5) {
    std::vector<std::string> items;
    for (int i = 0; i < n; i++)
        items.push_back("Item " + std::to_string(i));
    return std::make_shared<StringListAdapter>(std::move(items));
}

TEST_CASE("ListView default state", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    REQUIRE(lv.selected_index() == -1);
    REQUIRE(lv.selection().empty());
    REQUIRE(lv.focusable() == true);
    REQUIRE(lv.multi_select() == false);
    REQUIRE(lv.alternating_row_colors() == false);
}

TEST_CASE("ListView set_selected", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    lv.set_selected(2);
    REQUIRE(lv.selected_index() == 2);
    REQUIRE(lv.is_selected(2) == true);
    REQUIRE(lv.is_selected(0) == false);
    REQUIRE(lv.selection().size() == 1);
}

TEST_CASE("ListView set_selected clears previous", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    lv.set_selected(0);
    lv.set_selected(3);
    REQUIRE(lv.selection().size() == 1);
    REQUIRE(lv.is_selected(0) == false);
    REQUIRE(lv.is_selected(3) == true);
}

TEST_CASE("ListView set_selected out of range clears", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    lv.set_selected(2);
    lv.set_selected(-1);
    REQUIRE(lv.selection().empty());
    REQUIRE(lv.selected_index() == -1);
}

TEST_CASE("ListView clear_selection", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    lv.set_selected(1);
    lv.clear_selection();
    REQUIRE(lv.selection().empty());
    REQUIRE(lv.selected_index() == -1);
}

TEST_CASE("ListView select_all", "[listview]") {
    auto adapter = make_adapter(3);
    ListView lv(adapter);
    lv.select_all();
    REQUIRE(lv.selection().size() == 3);
    REQUIRE(lv.is_selected(0));
    REQUIRE(lv.is_selected(1));
    REQUIRE(lv.is_selected(2));
}

TEST_CASE("ListView set_selection with explicit set", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    lv.set_selection({1, 3});
    REQUIRE(lv.selection().size() == 2);
    REQUIRE(lv.is_selected(1));
    REQUIRE(lv.is_selected(3));
    REQUIRE_FALSE(lv.is_selected(0));
}

TEST_CASE("ListView multi_select toggle", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    REQUIRE(lv.multi_select() == false);
    lv.set_multi_select(true);
    REQUIRE(lv.multi_select() == true);
}

TEST_CASE("ListView alternating_row_colors toggle", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    REQUIRE(lv.alternating_row_colors() == false);
    lv.set_alternating_row_colors(true);
    REQUIRE(lv.alternating_row_colors() == true);
}

TEST_CASE("ListView on_selection_changed fires", "[listview]") {
    auto adapter = make_adapter();
    ListView lv(adapter);
    int notified = -1;
    lv.on_selection_changed = [&](int idx) { notified = idx; };
    lv.set_selected(3);
    REQUIRE(notified == 3);
}

TEST_CASE("ListView set_adapter resets state", "[listview]") {
    auto adapter1 = make_adapter(3);
    auto adapter2 = make_adapter(5);
    ListView lv(adapter1);
    lv.set_selected(2);
    lv.set_adapter(adapter2);
    REQUIRE(lv.selected_index() == -1);
    REQUIRE(lv.selection().empty());
}

TEST_CASE("ListView with null adapter", "[listview]") {
    ListView lv(nullptr);
    REQUIRE(lv.selected_index() == -1);
    lv.set_selected(0);
    REQUIRE(lv.selected_index() == -1);
    lv.select_all();
    REQUIRE(lv.selection().empty());
}
