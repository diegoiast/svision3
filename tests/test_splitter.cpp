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
    auto w1_before = sp.child_at(0).lock()->rect().width;
    auto w2_before = sp.child_at(1).lock()->rect().width;

    sp.set_rect({0, 0, 1400, 500}); // +400 total, split equally: +200 each
    REQUIRE(sp.child_at(0).lock()->rect().width == w1_before + 200);
    REQUIRE(sp.child_at(1).lock()->rect().width == w2_before + 200);
}

TEST_CASE("Splitter stretch: factor 0 keeps a child fixed, the other absorbs everything",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch_factor(0, 0.0f);

    sp.set_rect({0, 0, 1000, 500});
    auto w1_before = sp.child_at(0).lock()->rect().width;
    auto w2_before = sp.child_at(1).lock()->rect().width;

    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0).lock()->rect().width == w1_before);
    REQUIRE(sp.child_at(1).lock()->rect().width == w2_before + 400);

    // Shrinking back down keeps the same fixed size too.
    sp.set_rect({0, 0, 800, 500});
    REQUIRE(sp.child_at(0).lock()->rect().width == w1_before);
    REQUIRE(sp.child_at(1).lock()->rect().width == w2_before - 200);
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
    auto h0_before = sp.child_at(0).lock()->rect().height;
    auto h1_before = sp.child_at(1).lock()->rect().height;
    auto h2_before = sp.child_at(2).lock()->rect().height;

    sp.set_rect({0, 0, 500, 1400}); // +400 total: child0 +0, child1 +100, child2 +300
    REQUIRE(sp.child_at(0).lock()->rect().height == h0_before);
    REQUIRE(sp.child_at(1).lock()->rect().height == h1_before + 100);
    REQUIRE(sp.child_at(2).lock()->rect().height == h2_before + 300);
}

TEST_CASE("Splitter stretch: a locked divider overrides stretch redistribution", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>());
    sp.add_child(std::make_unique<Label>());
    sp.set_ratio(0, 0.3f);
    sp.set_stretch_factor(0, 0.0f);

    sp.set_rect({0, 0, 1000, 500});
    sp.set_divider_locked(0, true);
    auto w1_locked = sp.child_at(0).lock()->rect().width;

    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0).lock()->rect().width == w1_locked);

    // Unlocking restores normal stretch-based redistribution.
    sp.set_divider_locked(0, false);
    auto w1_unlocked = sp.child_at(0).lock()->rect().width;
    sp.set_rect({0, 0, 1800, 500});
    REQUIRE(sp.child_at(0).lock()->rect().width == w1_unlocked);
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
    auto w1_dragged = sp.child_at(0).lock()->rect().width;

    // Child 0 still has stretch factor 0, so it keeps the size the user just
    // dragged it to; child 1 still absorbs the whole resize.
    sp.set_rect({0, 0, 1400, 500});
    REQUIRE(sp.child_at(0).lock()->rect().width == w1_dragged);
}

TEST_CASE("Splitter child_at: out-of-range index returns an empty reference", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("A"));

    REQUIRE_FALSE(sp.child_at(0).expired());
    REQUIRE(sp.child_at(0).lock().get() != nullptr);
    REQUIRE(sp.child_at(1).expired());
    REQUIRE(sp.child_at(1).lock().get() == nullptr);
}

TEST_CASE("Splitter remove_child: destroys the child and shifts later children down",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("A"));
    sp.add_child(std::make_unique<Label>("B"));
    sp.add_child(std::make_unique<Label>("C"));

    sp.remove_child(1);

    REQUIRE(sp.child_count() == 2);
    REQUIRE(static_cast<Label *>(sp.child_at(0).lock().get())->text() == "A");
    REQUIRE(static_cast<Label *>(sp.child_at(1).lock().get())->text() == "C");
}

TEST_CASE("Splitter remove_child: a reference held before removal goes empty, not dangling",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("A"));
    sp.add_child(std::make_unique<Label>("B"));

    auto ref = sp.child_at(0);
    REQUIRE_FALSE(ref.expired());

    sp.remove_child(0);
    REQUIRE(ref.expired());
    REQUIRE(ref.lock() == nullptr);
}

TEST_CASE("Splitter remove_child: out-of-range index is a no-op", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("A"));

    sp.remove_child(5);
    REQUIRE(sp.child_count() == 1);
}

TEST_CASE("Splitter insert_child: inserting at the front shifts existing children back",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("B"));
    sp.add_child(std::make_unique<Label>("C"));

    sp.insert_child(0, std::make_unique<Label>("A"));

    REQUIRE(sp.child_count() == 3);
    REQUIRE(static_cast<Label *>(sp.child_at(0).lock().get())->text() == "A");
    REQUIRE(static_cast<Label *>(sp.child_at(1).lock().get())->text() == "B");
    REQUIRE(static_cast<Label *>(sp.child_at(2).lock().get())->text() == "C");
}

TEST_CASE("Splitter insert_child: inserting in the middle preserves order on both sides",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("A"));
    sp.add_child(std::make_unique<Label>("C"));

    sp.insert_child(1, std::make_unique<Label>("B"));

    REQUIRE(sp.child_count() == 3);
    REQUIRE(static_cast<Label *>(sp.child_at(0).lock().get())->text() == "A");
    REQUIRE(static_cast<Label *>(sp.child_at(1).lock().get())->text() == "B");
    REQUIRE(static_cast<Label *>(sp.child_at(2).lock().get())->text() == "C");
}

TEST_CASE("Splitter insert_child: an index beyond child_count() clamps to append", "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("A"));

    sp.insert_child(50, std::make_unique<Label>("B"));

    REQUIRE(sp.child_count() == 2);
    REQUIRE(static_cast<Label *>(sp.child_at(1).lock().get())->text() == "B");
}

TEST_CASE("Splitter insert_child: the new divider produces a valid, in-bounds layout",
          "[splitter]") {
    Splitter sp(Orientation::Horizontal);
    sp.add_child(std::make_unique<Label>("A"));
    sp.add_child(std::make_unique<Label>("C"));
    sp.set_rect({0, 0, 900, 500});

    sp.insert_child(1, std::make_unique<Label>("B"));

    REQUIRE(sp.child_at(0).lock()->rect().width >= 0);
    REQUIRE(sp.child_at(1).lock()->rect().width >= 0);
    REQUIRE(sp.child_at(2).lock()->rect().width >= 0);
    auto total = sp.child_at(0).lock()->rect().width + sp.child_at(1).lock()->rect().width +
                 sp.child_at(2).lock()->rect().width;
    REQUIRE(total <= 900);
}
