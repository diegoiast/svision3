#include <catch2/catch_test_macros.hpp>
#include "toolkit/splitter.hpp"
#include "toolkit/label.hpp"

using namespace toolkit;

TEST_CASE("Splitter stretch: by default all children grow equally on resize", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.5f);
    REQUIRE(sp.stretch_factor(0) == 1.0f);
    REQUIRE(sp.stretch_factor(1) == 1.0f);

    sp.set_rect({0, 0, 1000, 500});
    auto w1_before = sp.child_at(0)->rect().width;
    auto w2_before = sp.child_at(1)->rect().width;

    sp.set_rect({0, 0, 1400, 500}); // +400 total, split equally: +200 each
    REQUIRE(sp.child_at(0)->rect().width == w1_before + 200);
    REQUIRE(sp.child_at(1)->rect().width == w2_before + 200);
}

TEST_CASE("Splitter stretch: factor 0 keeps a child fixed, the other absorbs everything",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch_factor(0, 0.0f);

    sp.set_rect({0, 0, 1000, 500});
    auto w1_before = sp.child_at(0)->rect().width;
    auto w2_before = sp.child_at(1)->rect().width;

    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_before);
    REQUIRE(sp.child_at(1)->rect().width == w2_before + 400);

    // Shrinking back down keeps the same fixed size too.
    sp.set_rect({0, 0, 800, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_before);
    REQUIRE(sp.child_at(1)->rect().width == w2_before - 200);
}

TEST_CASE("Splitter stretch: three children distribute a resize by relative factor",
          "[splitter]") {
    Splitter sp(Orientation::Vertical);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 1.0f / 3.0f);
    sp.set_ratio(1, 2.0f / 3.0f);
    sp.set_stretch_factor(0, 0.0f); // fixed
    sp.set_stretch_factor(1, 1.0f);
    sp.set_stretch_factor(2, 3.0f); // grows 3x as much as child 1

    sp.set_rect({0, 0, 500, 1000});
    auto h0_before = sp.child_at(0)->rect().height;
    auto h1_before = sp.child_at(1)->rect().height;
    auto h2_before = sp.child_at(2)->rect().height;

    sp.set_rect({0, 0, 500, 1400}); // +400 total: child0 +0, child1 +100, child2 +300
    REQUIRE(sp.child_at(0)->rect().height == h0_before);
    REQUIRE(sp.child_at(1)->rect().height == h1_before + 100);
    REQUIRE(sp.child_at(2)->rect().height == h2_before + 300);
}

TEST_CASE("Splitter stretch: a locked divider overrides stretch redistribution", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch_factor(0, 0.0f);

    sp.set_rect({0, 0, 1000, 500});
    sp.set_divider_locked(0, true);
    auto w1_locked = sp.child_at(0)->rect().width;

    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_locked);

    // Unlocking restores normal stretch-based redistribution.
    sp.set_divider_locked(0, false);
    auto w1_unlocked = sp.child_at(0)->rect().width;
    sp.set_rect({0, 0, 1800, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_unlocked);
}

TEST_CASE("Splitter stretch: dragging repositions locally, later resizes still redistribute",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch_factor(0, 0.0f);
    sp.set_rect({0, 0, 1000, 500});

    sp.handle_mouse({MouseEvent::Type::Press, {300, 10}});
    sp.handle_mouse({MouseEvent::Type::Drag, {500, 10}});
    sp.handle_mouse({MouseEvent::Type::Release, {500, 10}});
    auto w1_dragged = sp.child_at(0)->rect().width;

    // Child 0 still has stretch factor 0, so it keeps the size the user just
    // dragged it to; child 1 still absorbs the whole resize.
    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_dragged);
}
