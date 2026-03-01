#include <catch2/catch_test_macros.hpp>
#include "toolkit/layout.hpp"
#include "toolkit/button.hpp"
#include "toolkit/label.hpp"

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
