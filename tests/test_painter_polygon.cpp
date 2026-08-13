#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "svision3/painter.hpp"
#include <array>
#include <cmath>

using namespace svision3;
using Catch::Matchers::WithinAbs;

namespace {

// Records every triangle fill_polygon() decomposes into, so the tests can check
// coverage/area without depending on a real rendering backend.
class RecordingPainter : public Painter {
  public:
    std::vector<std::array<Point, 3>> triangles;

    void push_clip(Rect const &) override {}
    void pop_clip() override {}
    void push_translation(Point) override {}
    void pop_translation() override {}
    void push_rotation(float) override {}
    void pop_rotation() override {}
    void set_line_style(LineStyle) override {}
    void fill_rect(Rect const &, Color const &) override {}
    void draw_rect(Rect const &, Color const &, float) override {}
    void fill_rounded_rect(Rect const &, Color const &, float) override {}
    void draw_rounded_rect(Rect const &, Color const &, float, float) override {}
    void fill_triangle(Point a, Point b, Point c, Color const &) override {
        triangles.push_back({a, b, c});
    }
    void draw_line(Point, Point, Color const &, float) override {}
    void fill_circle(Point, float, Color const &) override {}
    void draw_circle(Point, float, Color const &, float) override {}
    void draw_image(ImageData const &, Point) override {}
    void draw_image_scaled(ImageData const &, Rect const &) override {}
    std::string_view name() const override { return "recording"; }
};

float triangle_area(Point a, Point b, Point c) {
    return std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) / 2.0f;
}

float polygon_area(std::vector<Point> const &pts) {
    float area = 0.0f;
    for (size_t i = 0; i < pts.size(); i++) {
        auto const &a = pts[i];
        auto const &b = pts[(i + 1) % pts.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return std::abs(area) / 2.0f;
}

float total_area(std::vector<std::array<Point, 3>> const &triangles) {
    float total = 0.0f;
    for (auto const &t : triangles) {
        total += triangle_area(t[0], t[1], t[2]);
    }
    return total;
}

} // namespace

TEST_CASE("fill_polygon ignores degenerate input", "[painter][polygon]") {
    RecordingPainter p;
    p.fill_polygon({}, Color::rgb(1, 0, 0));
    p.fill_polygon({{0, 0}, {1, 0}}, Color::rgb(1, 0, 0));
    REQUIRE(p.triangles.empty());
}

TEST_CASE("fill_polygon passes a triangle through unchanged", "[painter][polygon]") {
    RecordingPainter p;
    std::vector<Point> tri{{0, 0}, {10, 0}, {5, 10}};
    p.fill_polygon(tri, Color::rgb(1, 0, 0));

    REQUIRE(p.triangles.size() == 1);
    CHECK(p.triangles[0][0].x == tri[0].x);
    CHECK(p.triangles[0][0].y == tri[0].y);
    CHECK(p.triangles[0][1].x == tri[1].x);
    CHECK(p.triangles[0][2].x == tri[2].x);
}

TEST_CASE("fill_polygon covers a convex square exactly", "[painter][polygon]") {
    RecordingPainter p;
    std::vector<Point> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    p.fill_polygon(square, Color::rgb(0, 1, 0));

    REQUIRE(p.triangles.size() == 2); // n - 2 triangles for a simple polygon
    REQUIRE_THAT(total_area(p.triangles), WithinAbs(polygon_area(square), 1e-4f));
}

TEST_CASE("fill_polygon covers a concave L-shape exactly", "[painter][polygon]") {
    RecordingPainter p;
    // One reflex (concave) corner at {5, 5}; ear-clipping must route around it.
    std::vector<Point> l_shape{
        {0, 0}, {10, 0}, {10, 5}, {5, 5}, {5, 10}, {0, 10},
    };
    p.fill_polygon(l_shape, Color::rgb(0, 0, 1));

    REQUIRE(p.triangles.size() == 4); // n - 2 triangles for a simple polygon
    for (auto const &t : p.triangles) {
        REQUIRE(triangle_area(t[0], t[1], t[2]) > 0.0f); // no degenerate slivers
    }
    REQUIRE_THAT(total_area(p.triangles), WithinAbs(polygon_area(l_shape), 1e-4f));
}

TEST_CASE("fill_polygon area-under-curve strip used by area charts", "[painter][polygon]") {
    RecordingPainter p;
    // Line points (a non-convex zig-zag) followed by the baseline edge, the exact
    // shape AreaChart::paint() builds to fill the region under a curve.
    std::vector<Point> strip{
        {0, 5}, {10, 0}, {20, 8}, {30, 2}, {30, 20}, {0, 20},
    };
    p.fill_polygon(strip, Color::rgb(1, 1, 0));

    REQUIRE(p.triangles.size() == 4);
    for (auto const &t : p.triangles) {
        REQUIRE(triangle_area(t[0], t[1], t[2]) > 0.0f);
    }
    REQUIRE_THAT(total_area(p.triangles), WithinAbs(polygon_area(strip), 1e-3f));
}

TEST_CASE("fill_polygon works regardless of winding order", "[painter][polygon]") {
    RecordingPainter cw_painter;
    RecordingPainter ccw_painter;
    std::vector<Point> cw{{0, 0}, {0, 10}, {10, 10}, {10, 0}};
    std::vector<Point> ccw{{0, 0}, {10, 0}, {10, 10}, {0, 10}};

    cw_painter.fill_polygon(cw, Color::rgb(1, 1, 0));
    ccw_painter.fill_polygon(ccw, Color::rgb(1, 1, 0));

    REQUIRE_THAT(total_area(cw_painter.triangles), WithinAbs(100.0f, 1e-4f));
    REQUIRE_THAT(total_area(ccw_painter.triangles), WithinAbs(100.0f, 1e-4f));
}
