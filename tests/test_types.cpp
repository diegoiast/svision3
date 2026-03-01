#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "toolkit/types.hpp"

using namespace toolkit;
using Catch::Matchers::WithinAbs;

TEST_CASE("Point default construction", "[types]") {
    Point p;
    REQUIRE(p.x == 0.0f);
    REQUIRE(p.y == 0.0f);
}

TEST_CASE("Size default construction", "[types]") {
    Size s;
    REQUIRE(s.width == 0.0f);
    REQUIRE(s.height == 0.0f);
}

TEST_CASE("Rect::contains", "[types]") {
    Rect r{10, 20, 100, 50};

    REQUIRE(r.contains({10, 20}));
    REQUIRE(r.contains({60, 45}));
    REQUIRE(r.contains({110, 70}));

    REQUIRE_FALSE(r.contains({9, 25}));
    REQUIRE_FALSE(r.contains({111, 25}));
    REQUIRE_FALSE(r.contains({50, 19}));
    REQUIRE_FALSE(r.contains({50, 71}));
}

TEST_CASE("Color::rgb and Color::rgba", "[types]") {
    auto c = Color::rgb(0.5f, 0.6f, 0.7f);
    REQUIRE(c.r == 0.5f);
    REQUIRE(c.g == 0.6f);
    REQUIRE(c.b == 0.7f);
    REQUIRE(c.a == 1.0f);

    auto c2 = Color::rgba(0.1f, 0.2f, 0.3f, 0.4f);
    REQUIRE(c2.a == 0.4f);
}

TEST_CASE("Color::darken clamps to zero", "[types]") {
    auto c = Color::rgb(0.1f, 0.2f, 0.3f);
    auto d = c.darken(0.5f);
    REQUIRE(d.r == 0.0f);
    REQUIRE(d.g == 0.0f);
    REQUIRE(d.b == 0.0f);
    REQUIRE(d.a == 1.0f);
}

TEST_CASE("Color::lighten clamps to one", "[types]") {
    auto c = Color::rgb(0.8f, 0.9f, 1.0f);
    auto l = c.lighten(0.5f);
    REQUIRE(l.r == 1.0f);
    REQUIRE(l.g == 1.0f);
    REQUIRE(l.b == 1.0f);
}

TEST_CASE("Color::darken and lighten are inverse-ish", "[types]") {
    auto c = Color::rgb(0.5f, 0.5f, 0.5f);
    auto d = c.darken(0.1f);
    auto l = d.lighten(0.1f);
    REQUIRE_THAT(l.r, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(l.g, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(l.b, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("Margins construction", "[types]") {
    Margins m{1, 2, 3, 4};
    REQUIRE(m.top == 1.0f);
    REQUIRE(m.right == 2.0f);
    REQUIRE(m.bottom == 3.0f);
    REQUIRE(m.left == 4.0f);
}
