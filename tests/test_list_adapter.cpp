#include <catch2/catch_test_macros.hpp>
#include "svision3/item_model.hpp"

using namespace svision3;

TEST_CASE("StringListModel empty", "[adapter]") {
    StringListModel model;
    REQUIRE(model.row_count() == 0);
    REQUIRE(model.cell_text(0, 0).empty());
}

TEST_CASE("StringListModel with initial items", "[adapter]") {
    StringListModel model({"A", "B", "C"});
    REQUIRE(model.row_count() == 3);
    REQUIRE(model.cell_text(0, 0) == "A");
    REQUIRE(model.cell_text(1, 0) == "B");
    REQUIRE(model.cell_text(2, 0) == "C");
}

TEST_CASE("StringListModel cell_text out of range", "[adapter]") {
    StringListModel model({"A"});
    REQUIRE(model.cell_text(1, 0).empty());
    REQUIRE(model.cell_text(100, 0).empty());
}

TEST_CASE("StringListModel append", "[adapter]") {
    StringListModel model;
    model.append("First");
    model.append("Second");
    REQUIRE(model.row_count() == 2);
    REQUIRE(model.cell_text(0, 0) == "First");
    REQUIRE(model.cell_text(1, 0) == "Second");
}

TEST_CASE("StringListModel remove", "[adapter]") {
    StringListModel model({"A", "B", "C"});
    model.remove(1);
    REQUIRE(model.row_count() == 2);
    REQUIRE(model.cell_text(0, 0) == "A");
    REQUIRE(model.cell_text(1, 0) == "C");
}

TEST_CASE("StringListModel remove out of range does nothing", "[adapter]") {
    StringListModel model({"A", "B"});
    model.remove(5);
    REQUIRE(model.row_count() == 2);
}

TEST_CASE("StringListModel set_items replaces all", "[adapter]") {
    StringListModel model({"A", "B"});
    model.set_items({"X", "Y", "Z"});
    REQUIRE(model.row_count() == 3);
    REQUIRE(model.cell_text(0, 0) == "X");
}

TEST_CASE("StringListModel on_data_changed fires", "[adapter]") {
    StringListModel model;
    int notify_count = 0;
    model.on_data_changed = [&] { notify_count++; };

    model.append("A");
    REQUIRE(notify_count == 1);
    model.append("B");
    REQUIRE(notify_count == 2);
    model.remove(0);
    REQUIRE(notify_count == 3);
    model.set_items({"X"});
    REQUIRE(notify_count == 4);
}
