#include <catch2/catch_test_macros.hpp>
#include "toolkit/spin_box.hpp"

using namespace toolkit;

TEST_CASE("SpinBox default value", "[spinbox]") {
    SpinBox sb;
    REQUIRE(sb.value() == 0);
}

TEST_CASE("SpinBox initial value", "[spinbox]") {
    SpinBox sb(42, 0, 100);
    REQUIRE(sb.value() == 42);
}

TEST_CASE("SpinBox set_value clamps to range", "[spinbox]") {
    SpinBox sb(0, 10, 50);
    sb.set_value(5);
    REQUIRE(sb.value() == 10);
    sb.set_value(100);
    REQUIRE(sb.value() == 50);
    sb.set_value(30);
    REQUIRE(sb.value() == 30);
}

TEST_CASE("SpinBox set_range adjusts value", "[spinbox]") {
    SpinBox sb(50, 0, 100);
    sb.set_range(0, 40);
    REQUIRE(sb.value() == 40);
}

TEST_CASE("SpinBox Up/Down keys", "[spinbox]") {
    SpinBox sb(10, 0, 20, 3);
    sb.set_rect({0, 0, 100, 30});

    KeyEvent up{KeyEvent::Type::Press, Key::Up};
    sb.handle_key(up);
    REQUIRE(sb.value() == 13);

    KeyEvent down{KeyEvent::Type::Press, Key::Down};
    sb.handle_key(down);
    REQUIRE(sb.value() == 10);
}

TEST_CASE("SpinBox Up/Down clamps", "[spinbox]") {
    SpinBox sb(19, 0, 20, 5);
    KeyEvent up{KeyEvent::Type::Press, Key::Up};
    sb.handle_key(up);
    REQUIRE(sb.value() == 20);

    SpinBox sb2(1, 0, 20, 5);
    KeyEvent down{KeyEvent::Type::Press, Key::Down};
    sb2.handle_key(down);
    REQUIRE(sb2.value() == 0);
}

TEST_CASE("SpinBox on_change fires on step", "[spinbox]") {
    SpinBox sb(5, 0, 10, 1);
    int received = -1;
    sb.on_change = [&](int v) { received = v; };

    KeyEvent up{KeyEvent::Type::Press, Key::Up};
    sb.handle_key(up);
    REQUIRE(received == 6);
}

TEST_CASE("SpinBox on_change does not fire when clamped at limit", "[spinbox]") {
    SpinBox sb(10, 0, 10, 1);
    int count = 0;
    sb.on_change = [&](int) { count++; };

    KeyEvent up{KeyEvent::Type::Press, Key::Up};
    sb.handle_key(up);
    REQUIRE(count == 0);
    REQUIRE(sb.value() == 10);
}

TEST_CASE("SpinBox set_value does not fire on_change", "[spinbox]") {
    SpinBox sb(0, 0, 100);
    int count = 0;
    sb.on_change = [&](int) { count++; };
    sb.set_value(50);
    REQUIRE(count == 0);
    REQUIRE(sb.value() == 50);
}

TEST_CASE("SpinBox is focusable", "[spinbox]") {
    SpinBox sb;
    REQUIRE(sb.focusable());
}

TEST_CASE("SpinBox size_hint has positive dimensions", "[spinbox]") {
    SpinBox sb(0, 0, 9999);
    auto sz = sb.size_hint();
    REQUIRE(sz.width > 0);
    REQUIRE(sz.height > 0);
}
