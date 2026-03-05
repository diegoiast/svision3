#include <catch2/catch_test_macros.hpp>
#include "toolkit/radio_button.hpp"

using namespace toolkit;

TEST_CASE("RadioButton default not selected", "[radio]") {
    RadioGroup group;
    RadioButton rb("Option", group);
    REQUIRE(rb.selected() == false);
}

TEST_CASE("RadioGroup select", "[radio]") {
    RadioGroup group;
    RadioButton rb1("A", group);
    RadioButton rb2("B", group);
    RadioButton rb3("C", group);

    group.select(&rb2);
    REQUIRE(rb1.selected() == false);
    REQUIRE(rb2.selected() == true);
    REQUIRE(rb3.selected() == false);
}

TEST_CASE("RadioGroup mutual exclusivity", "[radio]") {
    RadioGroup group;
    RadioButton rb1("A", group);
    RadioButton rb2("B", group);

    group.select(&rb1);
    REQUIRE(rb1.selected() == true);
    REQUIRE(rb2.selected() == false);

    group.select(&rb2);
    REQUIRE(rb1.selected() == false);
    REQUIRE(rb2.selected() == true);
}

TEST_CASE("RadioGroup on_change callback", "[radio]") {
    RadioGroup group;
    RadioButton rb1("A", group);
    RadioButton rb2("B", group);
    RadioButton rb3("C", group);

    int changed_index = -1;
    group.on_change = [&](int idx) { changed_index = idx; };

    group.select(&rb1);
    REQUIRE(changed_index == 0);
    group.select(&rb3);
    REQUIRE(changed_index == 2);
}

TEST_CASE("RadioButton is focusable", "[radio]") {
    RadioGroup group;
    RadioButton rb("Option", group);
    REQUIRE(rb.focusable() == true);
}

TEST_CASE("RadioButton relative coordinates", "[radio]") {
    RadioGroup group;
    RadioButton rb("Option", group);
    rb.set_rect({100, 100, 200, 30});
    
    REQUIRE(rb.use_relative_coordinates() == true);
    
    MouseEvent e{};
    e.type = MouseEvent::Type::Press;
    
    // Relative position (10, 10) should succeed
    e.position = {10, 10};
    REQUIRE(rb.handle_mouse(e) == true);
    REQUIRE(rb.selected() == true);
    
    // Absolute position (110, 110) should fail
    RadioButton rb2("Option 2", group);
    rb2.set_rect({100, 150, 200, 30});
    e.position = {110, 160};
    REQUIRE(rb2.handle_mouse(e) == false);
}
