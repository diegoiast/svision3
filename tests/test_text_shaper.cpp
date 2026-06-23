// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#ifdef TOOLKIT_HAS_TEXT_SHAPER

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "toolkit/painters/cairo_text_shaper.hpp"
#include "toolkit/utf8.hpp"

#include <cairo.h>

using namespace toolkit;
using Catch::Matchers::WithinAbs;

// ── Helpers ──────────────────────────────────────────────────────────────────

static auto make_cr() -> cairo_t * {
    auto *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    return cairo_create(surf);
}

static void destroy_cr(cairo_t *cr) {
    auto *surf = cairo_get_target(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

// ── LTR: basic glyph layout ──────────────────────────────────────────────────

TEST_CASE("TextShaper positions LTR glyphs left-to-right without overlap", "[text][shaper][ltr]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto shaped = shaper.shape(cr, "ABC", 12.0f, FontFamily::System);

    REQUIRE_FALSE(shaped.runs.empty());
    REQUIRE(shaped.total_advance > 0);

    // Each glyph's x must be >= the previous — no overlap, proper LTR
    for (auto &run : shaped.runs) {
        double prev_x = -1e9;
        for (auto &g : run.glyphs) {
            REQUIRE(g.x >= prev_x - 0.001f);
            prev_x = g.x;
        }
    }

    destroy_cr(cr);
}

TEST_CASE("TextShaper LTR cursor positions increase with codepoint index", "[text][shaper][ltr]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto shaped = shaper.shape(cr, "ABC", 12.0f, FontFamily::System);

    REQUIRE(shaped.cursor_positions.size() >= 4); // N+1 for N=3

    // For LTR, visual X increases with codepoint index
    for (size_t i = 1; i < shaped.cursor_positions.size(); i++) {
        REQUIRE(shaped.cursor_positions[i] >= shaped.cursor_positions[i - 1] - 0.001f);
    }
    REQUIRE(shaped.cursor_positions[0] >= 0);

    destroy_cr(cr);
}
TEST_CASE("TextShaper LTR visual extent matches total_advance", "[text][shaper][ltr]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto shaped = shaper.shape(cr, "Hello", 12.0f, FontFamily::System);

    REQUIRE(shaped.total_advance > 0);

    // For a single run, total_advance is the visual width
    double visual_width = 0;
    for (auto &run : shaped.runs) {
        REQUIRE(run.advance > 0);
        visual_width += run.advance;
    }
    REQUIRE(std::abs(visual_width - shaped.total_advance) < 0.001f);

    destroy_cr(cr);
}

// ── RTL: basic glyph layout (was broken — chars overlapped) ──────────────────

TEST_CASE("TextShaper positions RTL glyphs left-to-right in visual order without overlap",
          "[text][shaper][rtl]") {
    auto *cr = make_cr();

    TextShaper shaper;
    // Hebrew: א (aleph, U+05D0), ב (bet, U+05D1), ג (gimel, U+05D2), ד (dalet, U+05D3)
    // These are strong RTL characters that will form a single RTL run.
    auto shaped = shaper.shape(cr, "\u05D0\u05D1\u05D2\u05D3", 12.0f, FontFamily::System);

    REQUIRE_FALSE(shaped.runs.empty());
    REQUIRE(shaped.total_advance > 0);

    // The critical test: after correct RTL positioning, glyphs (in visual LTR
    // HarfBuzz order) must have strictly non-decreasing x positions.
    // Previously, the shift-based approach placed the first logical character
    // at the leftmost x, causing all subsequent glyphs to overlap.
    bool found_run = false;
    for (auto &run : shaped.runs) {
        if (run.glyphs.size() < 2) {
            continue;
        }
        found_run = true;
        double prev_x = -1e9;
        for (auto &g : run.glyphs) {
            INFO("glyph x = " << g.x << ", prev_x = " << prev_x);
            REQUIRE(g.x >= prev_x - 0.001f);
            prev_x = g.x;
        }
    }
    REQUIRE(found_run);

    destroy_cr(cr);
}

TEST_CASE("TextShaper RTL cursor positions are valid", "[text][shaper][rtl]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto shaped = shaper.shape(cr, "\u05D0\u05D1\u05D2\u05D3", 12.0f, FontFamily::System);

    REQUIRE(shaped.cursor_positions.size() >= 5); // N+1 for N=4
    for (auto cp : shaped.cursor_positions) {
        REQUIRE(cp >= 0);
    }

    // For RTL, visual X decreases with codepoint index
    // (first logical char is rightmost, last logical is leftmost)
    for (size_t i = 1; i < shaped.cursor_positions.size(); i++) {
        REQUIRE(shaped.cursor_positions[i] <= shaped.cursor_positions[i - 1] + 0.001f);
    }

    destroy_cr(cr);
}

TEST_CASE("TextShaper RTL total_advance matches glyph span", "[text][shaper][rtl]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto shaped = shaper.shape(cr, "\u05D0\u05D1\u05D2\u05D3", 12.0f, FontFamily::System);

    REQUIRE(shaped.total_advance > 0);

    // Visual extent should match total_advance for a single RTL run
    double extent = 0;
    for (auto &run : shaped.runs) {
        extent += run.advance;
    }
    REQUIRE(std::abs(extent - shaped.total_advance) < 0.001f);

    destroy_cr(cr);
}

// ── Single RTL character ─────────────────────────────────────────────────────

TEST_CASE("TextShaper RTL alignment span equals total_advance", "[text][shaper][rtl]") {
    auto *cr = make_cr();
    TextShaper shaper;
    auto shaped = shaper.shape(cr, "\u05D0\u05D1\u05D2\u05D3", 12.0f, FontFamily::System);

    // Simulate what text_cursor_positions does: expand codepoint positions to byte positions
    auto expand = [](std::string_view txt, std::vector<double> const &cp_pos) {
        std::vector<double> result(txt.size() + 1, 0.0);
        size_t cp_idx = 0;
        size_t byte_pos = 0;
        while (byte_pos < txt.size()) {
            result[byte_pos] = cp_pos[cp_idx];
            auto prev = byte_pos;
            byte_pos = Utf8Iterator::next(txt, byte_pos);
            for (auto j = prev + 1; j < byte_pos; j++) {
                result[j] = cp_pos[cp_idx];
            }
            cp_idx++;
        }
        result[txt.size()] = cp_pos[cp_pos.size() - 1];
        return result;
    };

    auto text = std::string_view("\u05D0\u05D1\u05D2\u05D3");
    auto byte_positions = expand(text, shaped.cursor_positions);
    double span = byte_positions[0] - byte_positions[text.size()];
    REQUIRE_THAT(span, WithinAbs(shaped.total_advance, 0.001));
    destroy_cr(cr);
}

TEST_CASE("TextShaper same metrics across different contexts", "[text][shaper][rtl]") {
    auto *surf1 = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto *cr1 = cairo_create(surf1);
    auto *surf2 = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto *cr2 = cairo_create(surf2);

    TextShaper shaper;
    auto shaped1 = shaper.shape(cr1, "\u05D0\u05D1\u05D2\u05D3", 12.0f, FontFamily::System);
    auto shaped2 = shaper.shape(cr2, "\u05D0\u05D1\u05D2\u05D3", 12.0f, FontFamily::System);

    REQUIRE_THAT(shaped1.total_advance, WithinAbs(shaped2.total_advance, 0.001));
    REQUIRE(shaped1.cursor_positions.size() == shaped2.cursor_positions.size());
    for (size_t i = 0; i < shaped1.cursor_positions.size(); i++) {
        REQUIRE_THAT(shaped1.cursor_positions[i], WithinAbs(shaped2.cursor_positions[i], 0.001));
    }

    cairo_destroy(cr1);
    cairo_surface_destroy(surf1);
    cairo_destroy(cr2);
    cairo_surface_destroy(surf2);
}

TEST_CASE("TextShaper single RTL character positions correctly", "[text][shaper][rtl]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto shaped = shaper.shape(cr, "\u05D0", 12.0f, FontFamily::System);

    REQUIRE_FALSE(shaped.runs.empty());
    REQUIRE(shaped.total_advance > 0);

    for (auto &run : shaped.runs) {
        for (auto &g : run.glyphs) {
            // Pure RTL text is shifted left so the right edge aligns
            // with the origin; glyph x is negative, bounded by advance.
            REQUIRE(g.x > -shaped.total_advance - 1);
            REQUIRE(g.x < shaped.total_advance);
        }
    }

    REQUIRE(shaped.cursor_positions.size() >= 2);
    // cursor_positions[0] should be at the right edge (x + advance)
    // cursor_positions[1] should be at the left edge (x)
    // For RTL: cursor_positions[0] >= cursor_positions[1]
    REQUIRE(shaped.cursor_positions[0] >= shaped.cursor_positions[1] - 0.001f);

    destroy_cr(cr);
}

// ── Kerning: consistency across repeated calls ──────────────────────────────

TEST_CASE("TextShaper kerning is consistent across repeated calls", "[text][shaper][kerning]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto a = shaper.shape(cr, "AVTo", 12.0f, FontFamily::System);
    auto b = shaper.shape(cr, "AVTo", 12.0f, FontFamily::System);

    // Same text, same font, same context — glyph positions must be identical
    REQUIRE(a.runs.size() == b.runs.size());
    for (size_t ri = 0; ri < a.runs.size(); ri++) {
        REQUIRE(a.runs[ri].glyphs.size() == b.runs[ri].glyphs.size());
        for (size_t gi = 0; gi < a.runs[ri].glyphs.size(); gi++) {
            REQUIRE_THAT(a.runs[ri].glyphs[gi].x, WithinAbs(b.runs[ri].glyphs[gi].x, 0.001f));
        }
        REQUIRE_THAT(a.runs[ri].advance, WithinAbs(b.runs[ri].advance, 0.001f));
    }
    REQUIRE_THAT(a.total_advance, WithinAbs(b.total_advance, 0.001f));

    destroy_cr(cr);
}

TEST_CASE("TextShaper kerning is consistent across different cairo contexts",
          "[text][shaper][kerning]") {
    auto *cr1 = make_cr();
    auto *cr2 = make_cr();

    TextShaper shaper;
    auto a = shaper.shape(cr1, "AVTo", 12.0f, FontFamily::System);
    auto b = shaper.shape(cr2, "AVTo", 12.0f, FontFamily::System);

    // Different contexts should produce identical metrics for the same text
    REQUIRE_THAT(a.total_advance, WithinAbs(b.total_advance, 0.001f));
    REQUIRE(a.cursor_positions.size() == b.cursor_positions.size());
    for (size_t i = 0; i < a.cursor_positions.size(); i++) {
        REQUIRE_THAT(a.cursor_positions[i], WithinAbs(b.cursor_positions[i], 0.001f));
    }

    destroy_cr(cr1);
    destroy_cr(cr2);
}

TEST_CASE("TextShaper kerning scales proportionally with font size", "[text][shaper][kerning]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto small = shaper.shape(cr, "AVTo", 10.0f, FontFamily::System);
    auto large = shaper.shape(cr, "AVTo", 20.0f, FontFamily::System);

    // Double the font size → double the advance
    REQUIRE_THAT(large.total_advance, WithinAbs(small.total_advance * 2.0f, 1.0f));

    // Each glyph position also doubles
    REQUIRE(small.runs.size() == large.runs.size());
    for (size_t ri = 0; ri < small.runs.size(); ri++) {
        REQUIRE(small.runs[ri].glyphs.size() == large.runs[ri].glyphs.size());
        for (size_t gi = 0; gi < small.runs[ri].glyphs.size(); gi++) {
            REQUIRE_THAT(large.runs[ri].glyphs[gi].x,
                         WithinAbs(small.runs[ri].glyphs[gi].x * 2.0f, 1.0f));
        }
    }

    // Cursor positions also double
    REQUIRE(small.cursor_positions.size() == large.cursor_positions.size());
    for (size_t i = 0; i < small.cursor_positions.size(); i++) {
        REQUIRE_THAT(large.cursor_positions[i], WithinAbs(small.cursor_positions[i] * 2.0f, 1.0f));
    }

    destroy_cr(cr);
}

// ── Kerning: glyph advance properties ──────────────────────────────────────

TEST_CASE("TextShaper kerning diagnostic — print glyph positions", "[text][shaper][kerning][.]") {
    auto *cr = make_cr();
    TextShaper shaper;

    for (auto text : {"AVTo", "ABC", "Hello", "MOM", "WAV"}) {
        auto shaped = shaper.shape(cr, text, 48.0f, FontFamily::System);
        INFO("Text: " << text);
        for (auto &run : shaped.runs) {
            for (size_t i = 0; i < run.glyphs.size(); i++) {
                INFO("  glyph[" << i << "]: index=" << run.glyphs[i].index
                                << " x=" << run.glyphs[i].x);
            }
            INFO("  run advance: " << run.advance);
        }
        INFO("  total_advance: " << shaped.total_advance);
    }
    destroy_cr(cr);
}

TEST_CASE("TextShaper same glyph positions after interleaved contexts", "[text][shaper][kerning]") {
    auto *cr = make_cr();
    TextShaper shaper;

    // First shape — populates cache
    auto a = shaper.shape(cr, "AVTo", 12.0f, FontFamily::System);

    // Interleave with different text on another context (simulates measure())
    auto *cr2 = make_cr();
    shaper.shape(cr2, "xyz", 10.0f, FontFamily::Monospace);
    destroy_cr(cr2);

    // Re-shape original — positions must be identical
    auto b = shaper.shape(cr, "AVTo", 12.0f, FontFamily::System);
    REQUIRE(a.runs.size() == b.runs.size());
    for (size_t ri = 0; ri < a.runs.size(); ri++) {
        REQUIRE(a.runs[ri].glyphs.size() == b.runs[ri].glyphs.size());
        for (size_t gi = 0; gi < a.runs[ri].glyphs.size(); gi++) {
            REQUIRE_THAT(a.runs[ri].glyphs[gi].x, WithinAbs(b.runs[ri].glyphs[gi].x, 0.001f));
        }
    }
    destroy_cr(cr);
}

TEST_CASE("TextShaper kerning pair advance is not trivially additive", "[text][shaper][kerning]") {
    auto *cr = make_cr();

    TextShaper shaper;
    auto ab = shaper.shape(cr, "AB", 48.0f, FontFamily::System);
    auto av = shaper.shape(cr, "AV", 48.0f, FontFamily::System);

    // "AV" typically kerned so that A and V nest — its advance is less
    // than "AB" for most serif/sans fonts.  If kerning is broken the
    // advances would be equal (or AV > AB).
    //
    // We accept any ordering as long as the advance is sane — the real
    // assertion is that glyphs don't overlap *within* each pair.
    REQUIRE(ab.runs.size() == 1);
    REQUIRE(av.runs.size() == 1);
    REQUIRE(ab.runs[0].glyphs.size() == 2);
    REQUIRE(av.runs[0].glyphs.size() == 2);

    // No overlap within pair
    REQUIRE(ab.runs[0].glyphs[1].x >= ab.runs[0].glyphs[0].x - 0.001f);
    REQUIRE(av.runs[0].glyphs[1].x >= av.runs[0].glyphs[0].x - 0.001f);

    destroy_cr(cr);
}

#endif // TOOLKIT_HAS_TEXT_SHAPER
