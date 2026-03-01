#include <catch2/catch_test_macros.hpp>
#include "toolkit/combobox.hpp"

using namespace toolkit;

TEST_CASE("Combobox default state", "[combobox]") {
    Combobox cb;
    REQUIRE(cb.selected() == -1);
    REQUIRE(cb.selected_text().empty());
    REQUIRE(cb.cursor() == CursorShape::Hand);
}

TEST_CASE("Combobox with items", "[combobox]") {
    Combobox cb({"Apple", "Banana", "Cherry"});
    REQUIRE(cb.selected() == -1);

    cb.set_selected(1);
    REQUIRE(cb.selected() == 1);
    REQUIRE(cb.selected_text() == "Banana");
}

TEST_CASE("Combobox set_selected out of range", "[combobox]") {
    Combobox cb({"A", "B"});
    cb.set_selected(5);
    REQUIRE(cb.selected() == -1);

    cb.set_selected(-2);
    REQUIRE(cb.selected() == -1);
}

TEST_CASE("Combobox set_items replaces items", "[combobox]") {
    Combobox cb({"A", "B"});
    cb.set_selected(0);
    REQUIRE(cb.selected_text() == "A");

    cb.set_items({"X", "Y", "Z"});
    cb.set_selected(2);
    REQUIRE(cb.selected_text() == "Z");
}

TEST_CASE("Combobox set_selected does not fire on_change", "[combobox]") {
    Combobox cb({"A", "B", "C"});
    bool called = false;
    cb.on_change = [&](int) { called = true; };
    cb.set_selected(2);
    REQUIRE(called == false);
    REQUIRE(cb.selected() == 2);
}

TEST_CASE("Combobox is focusable", "[combobox]") {
    Combobox cb;
    REQUIRE(cb.focusable() == true);
}
