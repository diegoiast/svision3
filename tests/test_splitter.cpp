#include <catch2/catch_test_macros.hpp>
#include "toolkit/splitter.hpp"
#include "toolkit/label.hpp"

using namespace toolkit;

TEST_CASE("Splitter stretch: Second keeps first child fixed, second absorbs resize",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch(0, StretchSide::Second);

    sp.set_rect({0, 0, 1000, 500});
    auto w1_before = sp.child_at(0)->rect().width;
    auto w2_before = sp.child_at(1)->rect().width;

    sp.set_rect({0, 0, 1400, 500});
    auto w1_after = sp.child_at(0)->rect().width;
    auto w2_after = sp.child_at(1)->rect().width;

    REQUIRE(w1_after == w1_before);
    REQUIRE(w2_after == w2_before + 400);

    // Shrinking back down keeps the same fixed size too.
    sp.set_rect({0, 0, 800, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_before);
    REQUIRE(sp.child_at(1)->rect().width == w2_before - 200);
}

TEST_CASE("Splitter stretch: First keeps second child fixed, first absorbs resize",
          "[splitter]") {
    Splitter sp(Orientation::Vertical);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.7f);
    sp.set_stretch(0, StretchSide::First);

    sp.set_rect({0, 0, 500, 1000});
    auto h1_before = sp.child_at(0)->rect().height;
    auto h2_before = sp.child_at(1)->rect().height;

    sp.set_rect({0, 0, 500, 1300});
    auto h1_after = sp.child_at(0)->rect().height;
    auto h2_after = sp.child_at(1)->rect().height;

    REQUIRE(h2_after == h2_before);
    REQUIRE(h1_after == h1_before + 300);
}

TEST_CASE("Splitter stretch: default Both is proportional, not fixed", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.5f);
    REQUIRE(sp.stretch(0) == StretchSide::Both);

    sp.set_rect({0, 0, 1000, 500});
    auto w1_before = sp.child_at(0)->rect().width;

    sp.set_rect({0, 0, 2000, 500});
    auto w1_after = sp.child_at(0)->rect().width;

    // With proportional (default) behaviour both sides scale with the ratio,
    // so the first child does *not* keep its old pixel size.
    REQUIRE(w1_after != w1_before);
}

TEST_CASE("Splitter stretch: a locked divider overrides the stretch setting", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch(0, StretchSide::Second);

    sp.set_rect({0, 0, 1000, 500});
    sp.set_divider_locked(0, true);
    auto w1_locked = sp.child_at(0)->rect().width;

    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_locked);

    // Unlocking falls back to the stretch anchor: first child keeps whatever
    // pixel size it had when the stretch anchor was (re)captured.
    sp.set_divider_locked(0, false);
    auto w1_unlocked = sp.child_at(0)->rect().width;
    sp.set_rect({0, 0, 1800, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_unlocked);
}

TEST_CASE("Splitter stretch: dragging re-anchors the fixed side", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch(0, StretchSide::Second);
    sp.set_rect({0, 0, 1000, 500});

    sp.handle_mouse({MouseEvent::Type::Press, {300, 10}});
    sp.handle_mouse({MouseEvent::Type::Drag, {500, 10}});
    sp.handle_mouse({MouseEvent::Type::Release, {500, 10}});
    auto w1_dragged = sp.child_at(0)->rect().width;

    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0)->rect().width == w1_dragged);
}
