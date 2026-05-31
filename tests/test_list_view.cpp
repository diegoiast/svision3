#include "toolkit/list_view.hpp"
#include "toolkit/theme_factory.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

static std::shared_ptr<StringListModel> make_model(int n = 5) {
    std::vector<std::string> items;
    for (int i = 0; i < n; i++) {
        items.push_back("Item " + std::to_string(i));
    }
    return std::make_shared<StringListModel>(std::move(items));
}

TEST_CASE("ListView default state", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    REQUIRE(!lv.selected_index());
    REQUIRE(lv.selection().empty());
    REQUIRE(lv.is_focusable() == true);
    REQUIRE(lv.get_multi_select() == false);
    REQUIRE(lv.alternating_row_colors() == false);
}

TEST_CASE("ListView set_selected", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    lv.set_selected(size_t{2});
    REQUIRE(lv.selected_index() == size_t{2});
    REQUIRE(lv.is_selected(2) == true);
    REQUIRE(lv.is_selected(0) == false);
    REQUIRE(lv.selection().size() == 1);
}

TEST_CASE("ListView set_selected clears previous", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    lv.set_selected(size_t{0});
    lv.set_selected(size_t{3});
    REQUIRE(lv.selection().size() == 1);
    REQUIRE(lv.is_selected(0) == false);
    REQUIRE(lv.is_selected(3) == true);
}

TEST_CASE("ListView set_selected nullopt clears", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    lv.set_selected(size_t{2});
    lv.set_selected(std::nullopt);
    REQUIRE(lv.selection().empty());
    REQUIRE(!lv.selected_index());
}

TEST_CASE("ListView clear_selection", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    lv.set_selected(size_t{1});
    lv.clear_selection();
    REQUIRE(lv.selection().empty());
    REQUIRE(!lv.selected_index());
}

TEST_CASE("ListView select_all", "[listview]") {
    auto model = make_model(3);
    ListView lv(model);
    lv.select_all();
    REQUIRE(lv.selection().size() == 3);
    REQUIRE(lv.is_selected(0));
    REQUIRE(lv.is_selected(1));
    REQUIRE(lv.is_selected(2));
}

TEST_CASE("ListView set_selection with explicit set", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    lv.set_selection({1, 3});
    REQUIRE(lv.selection().size() == 2);
    REQUIRE(lv.is_selected(1));
    REQUIRE(lv.is_selected(3));
    REQUIRE_FALSE(lv.is_selected(0));
}

TEST_CASE("ListView multi_select toggle", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    REQUIRE(lv.get_multi_select() == false);
    lv.set_multi_select(true);
    REQUIRE(lv.get_multi_select() == true);
}

TEST_CASE("ListView alternating_row_colors toggle", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    REQUIRE(lv.alternating_row_colors() == false);
    lv.set_alternating_row_colors(true);
    REQUIRE(lv.alternating_row_colors() == true);
}

TEST_CASE("ListView on_selection_changed fires", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    std::optional<size_t> notified;
    lv.on_selection_changed = [&](std::optional<size_t> idx) { notified = idx; };
    lv.set_selected(size_t{3});
    REQUIRE(notified == size_t{3});
}

TEST_CASE("ListView set_model resets state", "[listview]") {
    auto model1 = make_model(3);
    auto model2 = make_model(5);
    ListView lv(model1);
    lv.set_selected(size_t{2});
    lv.set_model(model2);
    REQUIRE(!lv.selected_index());
    REQUIRE(lv.selection().empty());
}

TEST_CASE("ListView with null model", "[listview]") {
    ListView lv(nullptr);
    REQUIRE(!lv.selected_index());
    lv.set_selected(size_t{0});
    REQUIRE(!lv.selected_index());
    lv.select_all();
    REQUIRE(lv.selection().empty());
}

TEST_CASE("ListView relative coordinates", "[listview]") {
    auto model = make_model();
    ListView lv(model);
    lv.set_rect({100, 100, 200, 200});

    MouseEvent e{};
    e.type = MouseEvent::Type::Press;

    // Relative position (10, 10) should succeed (hitting first item)
    e.position = {10, 10};
    REQUIRE(lv.handle_mouse(e) == true);

    // Relative position (10, 190) should fail (hitting empty area below items)
    // 5 items * ~20px = 100px. 190px is empty area.
    e.position = {10, 190};
    REQUIRE(lv.handle_mouse(e) == false);
}

TEST_CASE("ListView scrollbar visibility based on theme", "[listview]") {
    auto model = make_model(20); // Should need scroll
    ListView lv(model);
    lv.set_rect({0, 0, 100, 100});

    SECTION("Inline scrollbars (default Material)") {
        auto t = ThemeFactory::create(ThemeStyle::Material, ColorScheme::Light);
        Theme::set_current(std::move(t));
        lv.on_theme_changed();

        REQUIRE(Theme::current().palette.inline_scrollbars == true);
        // We can't easily check vscroll_ private member here without making it public or using a friend,
        // but we can check if it's found at a point where it should be if it were visible.
        // Actually, we added find_focusable_at and widget_at overrides.
        
        // At (90, 50), it should be the ListView itself (or its items), not the scrollbar widget.
        // If inline scrollbars are used, no separate widget is at that position.
        REQUIRE(lv.widget_at({90, 50}) == &lv);
    }

    SECTION("Regular scrollbars (Win95)") {
        auto t = ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light);
        Theme::set_current(std::move(t));
        lv.on_theme_changed();

        REQUIRE(Theme::current().palette.inline_scrollbars == false);
        
        // At (90, 50), it should hit the scrollbar widget.
        // ListView is 100 wide. bw=1. sw=16. Scrollbar is at [100-1-16, 99] = [83, 99].
        auto *w = lv.widget_at({90, 50});
        REQUIRE(w != &lv);
        REQUIRE(w != nullptr);
        REQUIRE(w->class_name() == "Scrollbar");
    }
}
