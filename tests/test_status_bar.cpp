#include "toolkit/status_bar.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

TEST_CASE("StatusBar defaults to empty", "[status_bar]") {
    StatusBar bar;
    REQUIRE(bar.section("none") == nullptr);
}

TEST_CASE("StatusBar add_section creates a section", "[status_bar]") {
    StatusBar bar;
    auto &s = bar.add_section("test", "hello");
    REQUIRE(s.text() == "hello");
    REQUIRE(s.id() == "test");
    REQUIRE(s.is_visible());
    REQUIRE(bar.section("test") != nullptr);
    REQUIRE(bar.section("test")->text() == "hello");
}

TEST_CASE("StatusBar add_section replaces existing", "[status_bar]") {
    StatusBar bar;
    bar.add_section("test", "first");
    auto &s = bar.add_section("test", "second");
    REQUIRE(s.text() == "second");
    REQUIRE(bar.section("test")->text() == "second");
}

TEST_CASE("StatusBarSection show/hide", "[status_bar]") {
    StatusBar bar;
    auto &s = bar.add_section("x", "text");
    REQUIRE(s.is_visible());
    s.hide();
    REQUIRE_FALSE(s.is_visible());
    s.show();
    REQUIRE(s.is_visible());
}

TEST_CASE("StatusBarSection set_text", "[status_bar]") {
    StatusBar bar;
    auto &s = bar.add_section("x", "old");
    REQUIRE(s.text() == "old");
    s.set_text("new");
    REQUIRE(s.text() == "new");
}

TEST_CASE("StatusBarSection clear_effect", "[status_bar]") {
    StatusBar bar;
    auto &s = bar.add_section("x", "text");
    s.spinner(100);
    REQUIRE(s.display_text() != "text");
    s.clear_effect();
    REQUIRE(s.display_text() == "text");
}

TEST_CASE("StatusBar remove_section", "[status_bar]") {
    StatusBar bar;
    bar.add_section("a", "first");
    bar.add_section("b", "second");
    REQUIRE(bar.section("a") != nullptr);
    REQUIRE(bar.section("b") != nullptr);
    bar.remove_section("a");
    REQUIRE(bar.section("a") == nullptr);
    REQUIRE(bar.section("b") != nullptr);
}

TEST_CASE("StatusBar clear removes all sections", "[status_bar]") {
    StatusBar bar;
    bar.add_section("a", "first");
    bar.add_section("b", "second");
    bar.clear();
    REQUIRE(bar.section("a") == nullptr);
    REQUIRE(bar.section("b") == nullptr);
}

TEST_CASE("StatusBarSection display_text no effect returns text", "[status_bar]") {
    StatusBarSection s("x", "hello");
    REQUIRE(s.display_text() == "hello");
}

TEST_CASE("AppearEffect reveals characters over time", "[status_bar]") {
    AppearEffect e(250);
    REQUIRE(e.apply("hello", 0.0f) == "");
    REQUIRE(e.apply("hello", 0.24f) == "");
    REQUIRE(e.apply("hello", 0.26f) == "h");
    REQUIRE(e.apply("hello", 0.5f) == "he");
    REQUIRE(e.apply("hello", 0.75f) == "hel");
    REQUIRE(e.apply("hello", 1.0f) == "hell");
    REQUIRE(e.apply("hello", 2.0f) == "hello");
}

TEST_CASE("AppearEffect empty text", "[status_bar]") {
    AppearEffect e(250);
    REQUIRE(e.apply("", 10.0f) == "");
}

TEST_CASE("SpinnerEffect reveals text and shows spinner", "[status_bar]") {
    SpinnerEffect e(100);
    // t < 0.25: no chars revealed
    auto r0 = e.apply("test", 0.0f);
    REQUIRE(r0 == " /");
    auto r1 = e.apply("test", 0.1f);
    REQUIRE(r1 == " |");
    auto r2 = e.apply("test", 0.2f);
    REQUIRE(r2 == " \\");
    // t=0.35: 1 char revealed, frame 3
    auto r35 = e.apply("test", 0.35f);
    REQUIRE(r35 == "t -");
    // t=0.5: 2 chars revealed, frame 5%4=1
    auto r5 = e.apply("test", 0.5f);
    REQUIRE(r5 == "te |");
    // t=1.0: all 4 chars revealed, frame 10%4=2
    auto r10 = e.apply("test", 1.0f);
    REQUIRE(r10 == "test \\");
    // spinner still rotating after full reveal
    auto r11 = e.apply("test", 1.1f);
    REQUIRE(r11 == "test -");
}

TEST_CASE("PulseEffect obscures with * then reveals text", "[status_bar]") {
    PulseEffect e(100);
    // Phase 1: pulse * chars one by one
    auto r0 = e.apply("hi", 0.0f);
    REQUIRE(r0 == "*");
    auto r1 = e.apply("hi", 0.1f);
    REQUIRE(r1 == "**");
    // Phase 2: reveal original chars, replace *
    auto r2 = e.apply("hi", 0.2f);
    REQUIRE(r2 == "h*");
    auto r3 = e.apply("hi", 0.3f);
    REQUIRE(r3 == "hi");
}

TEST_CASE("PulseEffect is_done after full cycle", "[status_bar]") {
    PulseEffect e(100);
    REQUIRE_FALSE(e.is_done("hi", 0.0f));
    REQUIRE_FALSE(e.is_done("hi", 0.3f));
    REQUIRE(e.is_done("hi", 0.4f));
}

TEST_CASE("PulseEffect handles longer text", "[status_bar]") {
    PulseEffect e(100);
    // "test" = 4 chars, total steps = 8
    // Phase 1 (steps 0-3): show stars
    auto r0 = e.apply("test", 0.0f);
    REQUIRE(r0 == "*");
    auto r3 = e.apply("test", 0.3f);
    REQUIRE(r3 == "****");
    // Phase 2 (steps 4-7): reveal
    auto r4 = e.apply("test", 0.4f);
    REQUIRE(r4 == "t***");
    auto r5 = e.apply("test", 0.5f);
    REQUIRE(r5 == "te**");
    auto r6 = e.apply("test", 0.6f);
    REQUIRE(r6 == "tes*");
    auto r7 = e.apply("test", 0.7f);
    REQUIRE(r7 == "test");
    REQUIRE(e.is_done("test", 0.8f));
}

TEST_CASE("Effect intervals return correct values", "[status_bar]") {
    AppearEffect ae(250);
    REQUIRE(ae.interval() == Catch::Approx(0.25f));

    SpinnerEffect se(100);
    REQUIRE(se.interval() == Catch::Approx(0.1f));

    PulseEffect pe(100);
    REQUIRE(pe.interval() == Catch::Approx(0.1f));
}

TEST_CASE("Effect clone produces equivalent effect", "[status_bar]") {
    AppearEffect original(250);
    auto clone = original.clone();
    REQUIRE(clone->apply("test", 0.3f) == original.apply("test", 0.3f));
    REQUIRE(clone->interval() == original.interval());

    SpinnerEffect original_s(100);
    auto clone_s = original_s.clone();
    REQUIRE(clone_s->apply("test", 0.15f) == original_s.apply("test", 0.15f));

    PulseEffect original_p(100);
    auto clone_p = original_p.clone();
    REQUIRE(clone_p->apply("x", 0.5f) == original_p.apply("x", 0.5f));
}

TEST_CASE("StatusBarSection appear method creates effect", "[status_bar]") {
    StatusBarSection s("x", "hello");
    s.appear(250);
    auto d0 = s.display_text();
    REQUIRE(d0 == "");
}

TEST_CASE("StatusBarSection set_effect accepts custom effect", "[status_bar]") {
    StatusBarSection s("x", "hello");
    s.set_effect(std::make_unique<AppearEffect>(250));
    REQUIRE(s.display_text() == "");
    REQUIRE_FALSE(s.is_effect_done());
}

TEST_CASE("StatusBarSection set_effect replaces existing effect", "[status_bar]") {
    StatusBarSection s("x", "test");
    s.spinner(100);
    REQUIRE(s.display_text() != "test");
    s.set_effect(std::make_unique<AppearEffect>(250));
    REQUIRE(s.display_text() == "");
}
