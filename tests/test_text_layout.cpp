// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/text/text_layout.hpp"
#include "svision3/utf8.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace svision3;
using namespace svision3::text;

namespace {

// Deterministic per-character shaper: every Unicode scalar is its own
// cluster, advance = 8px, regardless of script/font. Mirrors DummyRasterizer
// (svision3/text_rasterizer.hpp) so geometry assertions are exact integers.
class DummyTextShaper : public TextShaper {
  public:
    static constexpr float kCharWidth = 8.0f;

    std::vector<ClusterAdvance> shape_run(std::string_view run_utf8, bool rtl, float,
                                          FontFamily, bool = false, bool = false) override {
        std::vector<size_t> offs;
        size_t pos = 0;
        while (pos < run_utf8.size()) {
            offs.push_back(pos);
            pos = Utf8Iterator::next(run_utf8, pos);
        }

        std::vector<ClusterAdvance> result;
        result.reserve(offs.size());
        if (rtl) {
            for (size_t i = offs.size(); i-- > 0;) {
                result.push_back({offs[i], kCharWidth});
            }
        } else {
            for (auto off : offs) {
                result.push_back({off, kCharWidth});
            }
        }
        return result;
    }

    void draw_run(Painter &, std::string_view, bool, Point, Color const &, float,
                  FontFamily, bool = false, bool = false) override {}
};

} // namespace

// ── basic LTR / RTL sanity (no direction mixing) ───────────────────────────

TEST_CASE("TextLayout: pure LTR text lays out characters left to right at 8px each",
         "[text-layout][caret]") {
    DummyTextShaper shaper;
    TextLayout layout("Hello", bidi::BaseDirection::LTR, shaper, 14.0f);

    CHECK(layout.total_width() == 40.0f);
    CHECK(layout.caret_x(0) == 0.0f);
    CHECK(layout.caret_x(1) == 8.0f);
    CHECK(layout.caret_x(2) == 16.0f);
    CHECK(layout.caret_x(5) == 40.0f);
}

TEST_CASE("TextLayout: pure RTL text places start-of-string caret at the visual right edge",
         "[text-layout][caret]") {
    DummyTextShaper shaper;
    std::string text = "שלום";
    auto base = bidi::detect_base_direction(text);
    REQUIRE(base == bidi::BaseDirection::RTL);
    TextLayout layout(text, base, shaper, 14.0f);

    REQUIRE(layout.total_width() == 32.0f);
    CHECK(layout.caret_x(0) == 32.0f);
    CHECK(layout.caret_x(text.size()) == 0.0f);
}

// ── the canonical "ABD אבג 123" reference case (docs/design/rtl-line-input.md §1) ──

TEST_CASE("TextLayout: caret_x for the canonical mixed-direction reference case",
         "[text-layout][caret]") {
    DummyTextShaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);

    REQUIRE(layout.total_width() == 88.0f);

    auto const &offs = layout.bidi_line().char_offsets();
    REQUIRE(offs.size() == 12); // 11 chars + end

    // Visual order is "A B D _ 1 2 3 _ ג ב א" -- LTR prefix, then the LTR
    // digit run, then the RTL Hebrew+space run, each glued to its neighbour.
    CHECK(layout.caret_x(offs[0]) == 0.0f);  // before A
    CHECK(layout.caret_x(offs[1]) == 8.0f);  // before B
    CHECK(layout.caret_x(offs[2]) == 16.0f); // before D
    CHECK(layout.caret_x(offs[3]) == 24.0f); // before the first space
    CHECK(layout.caret_x(offs[4]) == 88.0f); // before א -- visual right edge of the RTL run
    CHECK(layout.caret_x(offs[5]) == 80.0f); // before ב
    CHECK(layout.caret_x(offs[6]) == 72.0f); // before ג
    CHECK(layout.caret_x(offs[7]) == 64.0f); // before the second space
    CHECK(layout.caret_x(offs[8]) == 32.0f); // before '1'
    CHECK(layout.caret_x(offs[9]) == 40.0f); // before '2'
    CHECK(layout.caret_x(offs[10]) == 48.0f); // before '3' -- caret-after-'2', the canonical assertion
    CHECK(layout.caret_x(offs[11]) == 56.0f); // end of string

    // Caret-after-'2' lands mid-string: 48/88 == 6/11, not at either end.
    CHECK(layout.caret_x(offs[10]) / layout.total_width() == Catch::Approx(6.0 / 11.0));
}

TEST_CASE("TextLayout: x strictly decreases across the RTL run as the logical index increases",
         "[text-layout][caret]") {
    DummyTextShaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);
    auto const &offs = layout.bidi_line().char_offsets();

    CHECK(layout.caret_x(offs[4]) > layout.caret_x(offs[5]));
    CHECK(layout.caret_x(offs[5]) > layout.caret_x(offs[6]));
    CHECK(layout.caret_x(offs[6]) > layout.caret_x(offs[7]));
}

TEST_CASE("TextLayout: runs() reports the three visual runs of the reference case in paint order",
         "[text-layout][runs]") {
    DummyTextShaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);

    REQUIRE(layout.runs().size() == 3);

    auto const &r0 = layout.runs()[0];
    CHECK_FALSE(r0.rtl);
    CHECK(r0.x == 0.0f);
    CHECK(r0.width == 32.0f); // "ABD "

    auto const &r1 = layout.runs()[1];
    CHECK_FALSE(r1.rtl);
    CHECK(r1.x == 32.0f);
    CHECK(r1.width == 24.0f); // "123"

    auto const &r2 = layout.runs()[2];
    CHECK(r2.rtl);
    CHECK(r2.x == 56.0f);
    CHECK(r2.width == 32.0f); // "אבג " (logical order; rendered right-to-left)
}

// ── index_from_x: inverse of caret_x ───────────────────────────────────────

TEST_CASE("TextLayout: index_from_x recovers exact boundaries by round-tripping caret_x",
         "[text-layout][hit-test]") {
    DummyTextShaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);
    auto const &offs = layout.bidi_line().char_offsets();

    for (auto b : offs) {
        CHECK(layout.index_from_x(layout.caret_x(b)) == b);
    }
}

TEST_CASE("TextLayout: index_from_x picks the nearer boundary off-center within a character",
         "[text-layout][hit-test]") {
    DummyTextShaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);
    auto const &offs = layout.bidi_line().char_offsets();

    // x=42 sits inside '2' ([40,48)) but only 2px past its left edge -- closer
    // to "before '2'" (offs[9] == 40) than to "before '3'" (offs[10] == 48).
    CHECK(layout.index_from_x(42.0f) == offs[9]);
}

// ── selection_rects: one rect within a run, two across a direction boundary ──

TEST_CASE("TextLayout: selection_rects returns a single rect within one run",
         "[text-layout][selection]") {
    DummyTextShaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);
    auto const &offs = layout.bidi_line().char_offsets();

    auto rects = layout.selection_rects(offs[0], offs[2]); // "AB"
    REQUIRE(rects.size() == 1);
    CHECK(rects[0].x == 0.0f);
    CHECK(rects[0].width == 16.0f);
}

TEST_CASE("TextLayout: selection_rects splits into two rects across an LTR/RTL boundary",
         "[text-layout][selection]") {
    DummyTextShaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);
    auto const &offs = layout.bidi_line().char_offsets();

    // Selecting "D␠אב" (logical chars 2..5) crosses the level-0/level-1
    // boundary; visually it lands as two disjoint slices.
    auto rects = layout.selection_rects(offs[2], offs[6]);
    REQUIRE(rects.size() == 2);
    CHECK(rects[0].x == 16.0f);
    CHECK(rects[0].width == 16.0f);
    CHECK(rects[1].x == 72.0f);
    CHECK(rects[1].width == 16.0f);
}

TEST_CASE("TextLayout: selection_rects on an empty range returns nothing",
         "[text-layout][selection]") {
    DummyTextShaper shaper;
    TextLayout layout("Hello", bidi::BaseDirection::LTR, shaper, 14.0f);
    CHECK(layout.selection_rects(2, 2).empty());
}
