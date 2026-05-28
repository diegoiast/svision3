#include <catch2/catch_test_macros.hpp>
#include "toolkit/button.hpp"
#include <nlohmann/json.hpp>

using namespace toolkit;

TEST_CASE("Button Serialization", "[button][serialization]") {
    Button b("Hello World");
    b.set_checked(true);
    b.set_checkable(true);
    b.set_flat(true);
    
    auto j = b.to_json();
    
    // Check if properties exist
    REQUIRE(j.contains("text"));
    REQUIRE(j["text"] == "Hello World");
    REQUIRE(j["checked"] == true);
    REQUIRE(j["checkable"] == true);
    REQUIRE(j["flat"] == true);
    
    // Test deserialization
    Button b2("");
    b2.from_json(j);
    
    REQUIRE(b2.text() == "Hello World");
    REQUIRE(b2.is_checked() == true);
    REQUIRE(b2.is_checkable() == true);
    REQUIRE(b2.is_flat() == true);
}
