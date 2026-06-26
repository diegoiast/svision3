// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/painters/win32_shaper.hpp"
#include "toolkit/text/text_layout.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

// Sanity checks for the real Uniscribe-backed shaper. Pixel values aren't
// asserted (they depend on whatever font is actually installed) -- these
// only check the structural contract text_layout.hpp documents: one
// ClusterAdvance per scalar, non-negative widths, and direction-consistent
// caret geometry when driving a real TextLayout. See test_text_layout.cpp
// for the pixel-exact version of these same checks against a DummyTextShaper.

using namespace toolkit;
using namespace toolkit::text;

TEST_CASE("Win32Shaper: shape_run covers every character of a plain LTR run", "[win32-shaper]") {
    Win32Shaper shaper;
    auto advances = shaper.shape_run("Hello", false, 14.0f, FontFamily::System);

    REQUIRE(advances.size() == 5);
    float total = 0.0f;
    for (auto const &a : advances) {
        CHECK(a.advance >= 0.0f);
        total += a.advance;
    }
    CHECK(total > 0.0f);
}

TEST_CASE("Win32Shaper: shape_run covers every character of a plain RTL run", "[win32-shaper]") {
    Win32Shaper shaper;
    std::string text = "שלום";
    auto advances = shaper.shape_run(text, true, 14.0f, FontFamily::System);

    REQUIRE(advances.size() == 4);
    for (auto const &a : advances) {
        CHECK(a.advance >= 0.0f);
    }
}

TEST_CASE("Win32Shaper: shape_run on an empty run returns nothing", "[win32-shaper]") {
    Win32Shaper shaper;
    CHECK(shaper.shape_run("", false, 14.0f, FontFamily::System).empty());
}

TEST_CASE("Win32Shaper: byte offsets are exactly the run's character boundaries",
         "[win32-shaper]") {
    Win32Shaper shaper;
    std::string text = "AB 12";
    auto advances = shaper.shape_run(text, false, 14.0f, FontFamily::System);

    std::vector<size_t> offs;
    for (auto const &a : advances) {
        offs.push_back(a.byte_offset);
    }
    std::sort(offs.begin(), offs.end());
    CHECK(offs == std::vector<size_t>{0, 1, 2, 3, 4});
}

TEST_CASE("Win32Shaper: TextLayout caret_x stays monotonic within each direction run for the "
         "canonical mixed-direction reference case",
         "[win32-shaper][text-layout]") {
    Win32Shaper shaper;
    std::string text = "ABD אבג 123";
    TextLayout layout(text, bidi::BaseDirection::LTR, shaper, 14.0f);
    auto const &offs = layout.bidi_line().char_offsets();
    REQUIRE(offs.size() == 12);
    REQUIRE(layout.runs().size() == 3);

    // "ABD " (LTR prefix) advances left to right.
    CHECK(layout.caret_x(offs[0]) <= layout.caret_x(offs[1]));
    CHECK(layout.caret_x(offs[1]) <= layout.caret_x(offs[2]));
    CHECK(layout.caret_x(offs[2]) <= layout.caret_x(offs[3]));

    // "אבג " (RTL run) decreases as the logical index increases -- it is laid
    // out right to left visually.
    CHECK(layout.caret_x(offs[4]) >= layout.caret_x(offs[5]));
    CHECK(layout.caret_x(offs[5]) >= layout.caret_x(offs[6]));
    CHECK(layout.caret_x(offs[6]) >= layout.caret_x(offs[7]));

    // "123" (LTR digit run) advances left to right.
    CHECK(layout.caret_x(offs[8]) <= layout.caret_x(offs[9]));
    CHECK(layout.caret_x(offs[9]) <= layout.caret_x(offs[10]));

    CHECK(layout.total_width() > 0.0f);
}
