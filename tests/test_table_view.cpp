#include "toolkit/table_view.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

using namespace toolkit;

static std::shared_ptr<StringTableModel> make_model() {
    return std::make_shared<StringTableModel>(std::vector<std::string>{"Name", "Age", "City"},
                                              std::vector<std::vector<std::string>>{
                                                  {"Alice", "30", "London"},
                                                  {"Bob", "25", "Paris"},
                                                  {"Charlie", "35", "Berlin"},
                                                  {"Diana", "28", "Tokyo"},
                                                  {"Eve", "32", "NYC"},
                                              });
}

TEST_CASE("StringTableModel basics", "[tableview]") {
    auto m = make_model();
    REQUIRE(m->row_count() == 5);
    REQUIRE(m->column_count() == 3);
    REQUIRE(std::string(m->header_text(0)) == "Name");
    REQUIRE(std::string(m->header_text(1)) == "Age");
    REQUIRE(std::string(m->header_text(2)) == "City");
    REQUIRE(std::string(m->cell_text(0, 0)) == "Alice");
    REQUIRE(std::string(m->cell_text(0, 2)) == "London");
    REQUIRE(std::string(m->cell_text(4, 1)) == "32");
}

TEST_CASE("StringTableModel out of range", "[tableview]") {
    auto m = make_model();
    REQUIRE(m->cell_text(-1, 0).empty());
    REQUIRE(m->cell_text(99, 0).empty());
    REQUIRE(m->cell_text(0, 99).empty());
    REQUIRE(m->header_text(-1).empty());
    REQUIRE(m->header_text(99).empty());
}

TEST_CASE("StringTableModel append and remove", "[tableview]") {
    auto m = make_model();
    REQUIRE(m->row_count() == 5);

    bool notified = false;
    m->on_data_changed = [&] { notified = true; };

    m->append_row({"Frank", "40", "Rome"});
    REQUIRE(m->row_count() == 6);
    REQUIRE(std::string(m->cell_text(5, 0)) == "Frank");
    REQUIRE(notified);

    notified = false;
    m->remove_row(0);
    REQUIRE(m->row_count() == 5);
    REQUIRE(std::string(m->cell_text(0, 0)) == "Bob");
    REQUIRE(notified);
}

TEST_CASE("StringTableModel set_data", "[tableview]") {
    auto m = make_model();
    bool notified = false;
    m->on_data_changed = [&] { notified = true; };

    m->set_data({"X", "Y"}, {{"1", "2"}, {"3", "4"}});
    REQUIRE(notified);
    REQUIRE(m->column_count() == 2);
    REQUIRE(m->row_count() == 2);
    REQUIRE(std::string(m->header_text(0)) == "X");
    REQUIRE(std::string(m->cell_text(1, 1)) == "4");
}

TEST_CASE("TableView default state", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    REQUIRE(tv.selected_row() == -1);
    REQUIRE(tv.selection().empty());
    REQUIRE(tv.is_focusable());
    REQUIRE_FALSE(tv.multi_select());
    REQUIRE_FALSE(tv.alternating_row_colors());
    REQUIRE(tv.sort_column() == -1);
    REQUIRE(tv.sort_order() == SortOrder::None);
}

TEST_CASE("TableView set_selected_row", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    tv.set_selected_row(2);
    REQUIRE(tv.selected_row() == 2);
    REQUIRE(tv.is_selected(2));
    REQUIRE_FALSE(tv.is_selected(0));
    REQUIRE(tv.selection().size() == 1);
}

TEST_CASE("TableView set_selected_row replaces previous", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    tv.set_selected_row(0);
    tv.set_selected_row(3);
    REQUIRE(tv.selection().size() == 1);
    REQUIRE_FALSE(tv.is_selected(0));
    REQUIRE(tv.is_selected(3));
}

TEST_CASE("TableView set_selected_row out of range clears", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    tv.set_selected_row(2);
    tv.set_selected_row(-1);
    REQUIRE(tv.selection().empty());
    REQUIRE(tv.selected_row() == -1);
}

TEST_CASE("TableView clear_selection", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    tv.set_selected_row(1);
    tv.clear_selection();
    REQUIRE(tv.selection().empty());
    REQUIRE(tv.selected_row() == -1);
}

TEST_CASE("TableView select_all", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    tv.select_all();
    REQUIRE(tv.selection().size() == 5);
    for (int i = 0; i < 5; i++) {
        REQUIRE(tv.is_selected(i));
    }
}

TEST_CASE("TableView set_selection explicit set", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    tv.set_selection({1, 3, 4});
    REQUIRE(tv.selection().size() == 3);
    REQUIRE(tv.is_selected(1));
    REQUIRE(tv.is_selected(3));
    REQUIRE(tv.is_selected(4));
    REQUIRE_FALSE(tv.is_selected(0));
}

TEST_CASE("TableView on_selection_changed fires", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    int notified = -1;
    tv.on_selection_changed = [&](int row) { notified = row; };
    tv.set_selected_row(3);
    REQUIRE(notified == 3);
}

TEST_CASE("TableView set_model resets state", "[tableview]") {
    auto m1 = make_model();
    auto m2 = std::make_shared<StringTableModel>(
        std::vector<std::string>{"A"}, std::vector<std::vector<std::string>>{{"1"}, {"2"}});
    TableView tv(m1);
    tv.set_selected_row(2);
    tv.set_model(m2);
    REQUIRE(tv.selected_row() == -1);
    REQUIRE(tv.selection().empty());
    REQUIRE(tv.model() == m2);
}

TEST_CASE("TableView with null model", "[tableview]") {
    TableView tv(nullptr);
    REQUIRE(tv.selected_row() == -1);
    tv.set_selected_row(0);
    REQUIRE(tv.selected_row() == -1);
    tv.select_all();
    REQUIRE(tv.selection().empty());
}

TEST_CASE("TableView column_width defaults and set", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    float default_w = tv.column_width(0);
    REQUIRE(default_w > 0);

    tv.set_column_width(1, 200.0f);
    REQUIRE(tv.column_width(1) == 200.0f);
    REQUIRE(tv.column_width(0) == default_w);
}

TEST_CASE("TableView column_width enforces minimum", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    tv.set_column_width(0, 5.0f);
    REQUIRE(tv.column_width(0) >= 40.0f);
}

TEST_CASE("TableView multi_select toggle", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    REQUIRE_FALSE(tv.multi_select());
    tv.set_multi_select(true);
    REQUIRE(tv.multi_select());
}

TEST_CASE("TableView alternating_row_colors toggle", "[tableview]") {
    auto m = make_model();
    TableView tv(m);
    REQUIRE_FALSE(tv.alternating_row_colors());
    tv.set_alternating_row_colors(true);
    REQUIRE(tv.alternating_row_colors());
}

TEST_CASE("TableView relative coordinates", "[tableview]") {
    auto model = std::make_shared<StringTableModel>(
        std::vector<std::string>{"Col 1"},
        std::vector<std::vector<std::string>>{{"A"}, {"B"}, {"C"}});
    TableView tv(model);
    tv.set_rect({100, 100, 200, 200});

    MouseEvent e{};
    e.type = MouseEvent::Type::Press;

    // Relative position (10, 30) should succeed (hitting a row)
    // Header is ~20px, first row ends at ~40px.
    e.position = {10, 30};
    REQUIRE(tv.handle_mouse(e) == true);

    // Absolute position (110, 150) should fail because it's outside local [0, 200]
    e.position = {110, 150};
    REQUIRE(tv.handle_mouse(e) == false);
}
