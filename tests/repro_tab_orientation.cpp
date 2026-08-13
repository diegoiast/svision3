#include "svision3/button.hpp"
#include "svision3/label.hpp"
#include "svision3/tab_widget.hpp"
#include "svision3/widget_loader.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace svision3;

TEST_CASE("TabWidget Orientation Serialization", "[tabwidget][serialization]") {
    TabWidget tw;
    tw.set_orientation(TabOrientation::South);
    tw.add_tab("Tab 1", std::make_unique<Label>("Content 1"));

    auto j = tw.to_json();

    // Verify orientation is in JSON
    REQUIRE(j.contains("orientation"));
    REQUIRE(j["orientation"] == static_cast<int>(TabOrientation::South));

    // Test loading back
    TabWidget tw2;
    tw2.from_json(j);
    REQUIRE(tw2.orientation() == TabOrientation::South);

    // Verify tabs are not doubled
    // (If tw2.from_json was called, it cleared and re-added tabs)
    // Actually, WidgetLoader::create_widget is the one adding them now.

    // Let's test via WidgetLoader to be sure
    auto tw3_ptr = WidgetLoader::instance().create_widget(j);
    auto *tw3 = dynamic_cast<TabWidget *>(tw3_ptr.get());
    REQUIRE(tw3 != nullptr);
    REQUIRE(tw3->get_tab_count() == 1); // Should be exactly 1
    REQUIRE(tw3->orientation() == TabOrientation::South);

    tw3->set_current(0);
    REQUIRE(tw3->get_current_index() == 0);
}
