#include <catch2/catch_test_macros.hpp>
#include "toolkit/list_view.hpp"

using namespace toolkit;

TEST_CASE("StringListAdapter empty", "[adapter]") {
    StringListAdapter adapter;
    REQUIRE(adapter.count() == 0);
    REQUIRE(adapter.text_at(0).empty());
    REQUIRE(adapter.text_at(-1).empty());
}

TEST_CASE("StringListAdapter with initial items", "[adapter]") {
    StringListAdapter adapter({"A", "B", "C"});
    REQUIRE(adapter.count() == 3);
    REQUIRE(adapter.text_at(0) == "A");
    REQUIRE(adapter.text_at(1) == "B");
    REQUIRE(adapter.text_at(2) == "C");
}

TEST_CASE("StringListAdapter text_at out of range", "[adapter]") {
    StringListAdapter adapter({"A"});
    REQUIRE(adapter.text_at(-1).empty());
    REQUIRE(adapter.text_at(1).empty());
    REQUIRE(adapter.text_at(100).empty());
}

TEST_CASE("StringListAdapter append", "[adapter]") {
    StringListAdapter adapter;
    adapter.append("First");
    adapter.append("Second");
    REQUIRE(adapter.count() == 2);
    REQUIRE(adapter.text_at(0) == "First");
    REQUIRE(adapter.text_at(1) == "Second");
}

TEST_CASE("StringListAdapter remove", "[adapter]") {
    StringListAdapter adapter({"A", "B", "C"});
    adapter.remove(1);
    REQUIRE(adapter.count() == 2);
    REQUIRE(adapter.text_at(0) == "A");
    REQUIRE(adapter.text_at(1) == "C");
}

TEST_CASE("StringListAdapter remove out of range does nothing", "[adapter]") {
    StringListAdapter adapter({"A", "B"});
    adapter.remove(-1);
    adapter.remove(5);
    REQUIRE(adapter.count() == 2);
}

TEST_CASE("StringListAdapter set_items replaces all", "[adapter]") {
    StringListAdapter adapter({"A", "B"});
    adapter.set_items({"X", "Y", "Z"});
    REQUIRE(adapter.count() == 3);
    REQUIRE(adapter.text_at(0) == "X");
}

TEST_CASE("StringListAdapter on_data_changed fires", "[adapter]") {
    StringListAdapter adapter;
    int notify_count = 0;
    adapter.on_data_changed = [&] { notify_count++; };

    adapter.append("A");
    REQUIRE(notify_count == 1);
    adapter.append("B");
    REQUIRE(notify_count == 2);
    adapter.remove(0);
    REQUIRE(notify_count == 3);
    adapter.set_items({"X"});
    REQUIRE(notify_count == 4);
}
