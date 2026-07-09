#include <catch2/catch_test_macros.hpp>
#include "toolkit/layout.hpp"
#include "toolkit/button.hpp"
#include "toolkit/label.hpp"
#include "toolkit/platform/dummy_platform.hpp"

using namespace toolkit;

TEST_CASE("VBoxLayout size_hint with no children", "[layout]") {
    VBoxLayout layout;
    auto hint = layout.size_hint();
    REQUIRE(hint.width == 0.0f);
    REQUIRE(hint.height == 0.0f);
}

TEST_CASE("VBoxLayout size_hint accumulates heights", "[layout]") {
    VBoxLayout layout;
    auto b1 = std::make_unique<Button>("A");
    auto b2 = std::make_unique<Button>("B");
    float h1 = b1->size_hint().height;
    float h2 = b2->size_hint().height;
    float w = std::max(b1->size_hint().width, b2->size_hint().width);
    layout.add_widget(std::move(b1));
    layout.add_widget(std::move(b2));

    auto hint = layout.size_hint();
    REQUIRE(hint.height == h1 + h2 + 8.0f);
    REQUIRE(hint.width == w);
}

TEST_CASE("VBoxLayout size_hint with margins", "[layout]") {
    VBoxLayout layout;
    layout.set_margins({10, 20, 30, 40});
    auto b = std::make_unique<Button>("X");
    float bh = b->size_hint().height;
    float bw = b->size_hint().width;
    layout.add_widget(std::move(b));

    auto hint = layout.size_hint();
    REQUIRE(hint.height == bh + 10 + 30);
    REQUIRE(hint.width == bw + 20 + 40);
}

TEST_CASE("VBoxLayout skips hidden widgets", "[layout]") {
    VBoxLayout layout;
    auto b1 = std::make_unique<Button>("A");
    auto b2 = std::make_unique<Button>("B");
    b2->hide();
    float h1 = b1->size_hint().height;
    layout.add_widget(std::move(b1));
    layout.add_widget(std::move(b2));

    auto hint = layout.size_hint();
    REQUIRE(hint.height == h1);
}

TEST_CASE("HBoxLayout size_hint accumulates widths", "[layout]") {
    HBoxLayout layout;
    auto b1 = std::make_unique<Button>("A");
    auto b2 = std::make_unique<Button>("B");
    float w1 = b1->size_hint().width;
    float w2 = b2->size_hint().width;
    float h = std::max(b1->size_hint().height, b2->size_hint().height);
    layout.add_widget(std::move(b1));
    layout.add_widget(std::move(b2));

    auto hint = layout.size_hint();
    REQUIRE(hint.width == w1 + w2 + 8.0f);
    REQUIRE(hint.height == h);
}

TEST_CASE("HBoxLayout skips hidden widgets", "[layout]") {
    HBoxLayout layout;
    auto b1 = std::make_unique<Button>("A");
    auto b2 = std::make_unique<Button>("B");
    b2->hide();
    float w1 = b1->size_hint().width;
    layout.add_widget(std::move(b1));
    layout.add_widget(std::move(b2));

    auto hint = layout.size_hint();
    REQUIRE(hint.width == w1);
}

TEST_CASE("VBoxLayout collect_focusables", "[layout]") {
    VBoxLayout layout;
    layout.add_widget(std::make_unique<Button>("A"));
    layout.add_widget(std::make_unique<Label>("text"));
    layout.add_widget(std::make_unique<Button>("B"));

    std::vector<Widget *> focusables;
    layout.collect_focusables(focusables);
    REQUIRE(focusables.size() == 2);
}

TEST_CASE("VBoxLayout collect_focusables skips hidden", "[layout]") {
    VBoxLayout layout;
    auto b = std::make_unique<Button>("A");
    b->hide();
    layout.add_widget(std::move(b));
    layout.add_widget(std::make_unique<Button>("B"));

    std::vector<Widget *> focusables;
    layout.collect_focusables(focusables);
    REQUIRE(focusables.size() == 1);
}

TEST_CASE("VBoxLayout collect_focusables skips disabled", "[layout]") {
    VBoxLayout layout;
    auto b = std::make_unique<Button>("A");
    b->set_enabled(false);
    layout.add_widget(std::move(b));

    std::vector<Widget *> focusables;
    layout.collect_focusables(focusables);
    REQUIRE(focusables.size() == 0);
}

TEST_CASE("VBoxLayout collect_mnemonics", "[layout]") {
    VBoxLayout layout;
    layout.add_widget(std::make_unique<Button>("&Save"));
    layout.add_widget(std::make_unique<Button>("Close"));
    layout.add_widget(std::make_unique<Button>("&Quit"));

    std::vector<Widget *> mnemonics;
    layout.collect_mnemonics(mnemonics);
    REQUIRE(mnemonics.size() == 2);
}

TEST_CASE("VBoxLayout respects margins for child positioning", "[layout]") {
    VBoxLayout layout;
    layout.set_margins({10, 20, 30, 40}); // top, right, bottom, left
    auto b = std::make_unique<Button>("X");
    auto *b_ptr = b.get();
    layout.add_widget(std::move(b), 0, Alignment::Start);
    
    layout.set_rect({0, 0, 100, 100});
    layout.find_focusable_at({0, 0}); // Trigger lazy layout
    
    REQUIRE(b_ptr->rect().y == 10);
    REQUIRE(b_ptr->rect().x == 40);
}

TEST_CASE("HBoxLayout respects margins for child positioning", "[layout]") {
    HBoxLayout layout;
    layout.set_margins({10, 20, 30, 40}); // top, right, bottom, left
    auto b = std::make_unique<Button>("X");
    auto *b_ptr = b.get();
    layout.add_widget(std::move(b), 0, Alignment::Start);
    
    layout.set_rect({0, 0, 100, 100});
    layout.find_focusable_at({0, 0}); // Trigger lazy layout
    
    REQUIRE(b_ptr->rect().y == 10);
    REQUIRE(b_ptr->rect().x == 40);
}

TEST_CASE("HBoxLayout fills vertically with Alignment::Fill", "[layout]") {
    HBoxLayout layout;
    layout.set_margins({10, 20, 30, 40}); // top, right, bottom, left
    auto b = std::make_unique<Button>("X");
    auto *b_ptr = b.get();
    // Default alignment should be Center in HBoxLayout, let's explicitly test Fill
    layout.add_widget(std::move(b), 0, Alignment::Fill);

    layout.set_rect({0, 0, 100, 100});
    layout.find_focusable_at({0, 0}); // Trigger lazy layout

    // content_h = 100 - 10 (top) - 30 (bottom) = 60
    REQUIRE(b_ptr->rect().y == 10);
    REQUIRE(b_ptr->rect().height == 60);
}

TEST_CASE("StackedLayout empty state", "[layout][stacked]") {
    StackedLayout sl;
    REQUIRE(sl.count() == 0);
    REQUIRE(sl.current() == -1);
}

TEST_CASE("StackedLayout add_widget sets current to first item", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A"));
    REQUIRE(sl.count() == 1);
    REQUIRE(sl.current() == 0);
    sl.add_widget(std::make_unique<Label>("B"));
    REQUIRE(sl.count() == 2);
    REQUIRE(sl.current() == 0); // first item stays current
}

TEST_CASE("StackedLayout only current item is visible", "[layout][stacked]") {
    StackedLayout sl;
    auto *a = &sl.add<Label>();
    auto *b = &sl.add<Label>();
    REQUIRE(a->is_visible());
    REQUIRE(!b->is_visible());

    sl.set_current(1);
    REQUIRE(!a->is_visible());
    REQUIRE(b->is_visible());
}

TEST_CASE("StackedLayout set_current ignores out-of-range", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A"));
    sl.add_widget(std::make_unique<Label>("B"));
    sl.set_current(1);
    REQUIRE(sl.current() == 1);
    sl.set_current(5);
    REQUIRE(sl.current() == 1);
    sl.set_current(-1);
    REQUIRE(sl.current() == 1);
}

TEST_CASE("StackedLayout apply_layout sizes current to full rect", "[layout][stacked]") {
    StackedLayout sl;
    sl.set_rect({0, 0, 200, 150}); // set rect before widgets
    auto *a = &sl.add<Label>();
    auto *b = &sl.add<Label>();

    REQUIRE(a->rect().x == 0.0f);
    REQUIRE(a->rect().y == 0.0f);
    REQUIRE(a->rect().width == 200.0f);
    REQUIRE(a->rect().height == 150.0f);

    sl.set_current(1);
    REQUIRE(b->rect().x == 0.0f);
    REQUIRE(b->rect().y == 0.0f);
    REQUIRE(b->rect().width == 200.0f);
    REQUIRE(b->rect().height == 150.0f);
}

TEST_CASE("StackedLayout apply_layout respects margins", "[layout][stacked]") {
    StackedLayout sl;
    sl.set_margins({10, 20, 30, 40}); // top, right, bottom, left
    sl.set_rect({0, 0, 200, 150});
    auto *a = &sl.add<Label>();

    REQUIRE(a->rect().x == 40.0f);
    REQUIRE(a->rect().y == 10.0f);
    REQUIRE(a->rect().width == 200.0f - 40.0f - 20.0f);
    REQUIRE(a->rect().height == 150.0f - 10.0f - 30.0f);
}

TEST_CASE("StackedLayout for_each_child visits all items including non-current", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A"));
    sl.add_widget(std::make_unique<Label>("B"));
    sl.add_widget(std::make_unique<Label>("C"));
    REQUIRE(sl.current() == 0);

    int visited = 0;
    sl.for_each_child([&](Widget *) { ++visited; });
    REQUIRE(visited == 3); // ALL items, not just the current one
}

TEST_CASE("StackedLayout remove_widget adjusts current when item removed after current", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A")); // 0, current
    sl.add_widget(std::make_unique<Label>("B")); // 1
    sl.add_widget(std::make_unique<Label>("C")); // 2

    sl.remove_widget(2); // remove after current
    REQUIRE(sl.count() == 2);
    REQUIRE(sl.current() == 0); // unchanged
}

TEST_CASE("StackedLayout remove_widget adjusts current when item removed before current", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A")); // 0
    sl.add_widget(std::make_unique<Label>("B")); // 1
    sl.set_current(1);

    sl.remove_widget(0); // remove before current
    REQUIRE(sl.count() == 1);
    REQUIRE(sl.current() == 0); // shifted down from 1
    REQUIRE(sl.items()[0]->is_visible());
}

TEST_CASE("StackedLayout remove_widget adjusts current when current item removed", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A")); // 0, current
    sl.add_widget(std::make_unique<Label>("B")); // 1

    sl.remove_widget(0); // remove current
    REQUIRE(sl.count() == 1);
    REQUIRE(sl.current() == 0); // clamped to new last item
    REQUIRE(sl.items()[0]->is_visible());
}

TEST_CASE("StackedLayout remove_widget all items yields current -1", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A"));
    sl.remove_widget(0);
    REQUIRE(sl.count() == 0);
    REQUIRE(sl.current() == -1);
}

TEST_CASE("StackedLayout swap_widgets exchanges items and tracks current", "[layout][stacked]") {
    StackedLayout sl;
    auto a = std::make_unique<Label>("A");
    auto b = std::make_unique<Label>("B");
    auto *a_ptr = a.get();
    auto *b_ptr = b.get();
    sl.add_widget(std::move(a)); // 0, current
    sl.add_widget(std::move(b)); // 1

    sl.swap_widgets(0, 1);
    REQUIRE(sl.items()[0].get() == b_ptr); // B is now at 0
    REQUIRE(sl.items()[1].get() == a_ptr); // A is now at 1
    REQUIRE(sl.current() == 1);           // current follows A to its new index

    // A should still be visible (it's current), B should not
    REQUIRE(a_ptr->is_visible());
    REQUIRE(!b_ptr->is_visible());
}

TEST_CASE("StackedLayout swap_widgets no-op for same index", "[layout][stacked]") {
    StackedLayout sl;
    sl.add_widget(std::make_unique<Label>("A"));
    sl.add_widget(std::make_unique<Label>("B"));
    sl.swap_widgets(0, 0); // no-op
    REQUIRE(sl.current() == 0);
    REQUIRE(sl.count() == 2);
}
