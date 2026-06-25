// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/text/bidi.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit::bidi;

// ── detect_base_direction (P2/P3: first strong character) ─────────────────────

TEST_CASE("detect_base_direction: empty string is LTR", "[bidi][direction]") {
    CHECK(detect_base_direction("") == BaseDirection::LTR);
}

TEST_CASE("detect_base_direction: pure LTR text", "[bidi][direction]") {
    CHECK(detect_base_direction("Hello") == BaseDirection::LTR);
}

TEST_CASE("detect_base_direction: pure Hebrew text is RTL", "[bidi][direction]") {
    CHECK(detect_base_direction("שלום") == BaseDirection::RTL);
}

TEST_CASE("detect_base_direction: pure Arabic text is RTL", "[bidi][direction]") {
    CHECK(detect_base_direction("مرحبا") == BaseDirection::RTL);
}

TEST_CASE("detect_base_direction: digits and spaces before a strong RTL character",
         "[bidi][direction]") {
    CHECK(detect_base_direction("123 שלום") == BaseDirection::RTL);
}

TEST_CASE("detect_base_direction: digits and spaces before a strong LTR character",
         "[bidi][direction]") {
    CHECK(detect_base_direction("123 hello") == BaseDirection::LTR);
}

TEST_CASE("detect_base_direction: punctuation-only text has no strong character, stays LTR",
         "[bidi][direction]") {
    CHECK(detect_base_direction("123 .,:+-%") == BaseDirection::LTR);
}

TEST_CASE("detect_base_direction: first strong character wins regardless of what follows",
         "[bidi][direction]") {
    CHECK(detect_base_direction("Aשלום") == BaseDirection::LTR);
    CHECK(detect_base_direction("שAלום") == BaseDirection::RTL);
}

// ── BidiLine::analyze: levels ──────────────────────────────────────────────────

TEST_CASE("BidiLine: empty string has no characters and no runs", "[bidi][levels]") {
    auto line = BidiLine::analyze("", BaseDirection::LTR);
    CHECK(line.char_count() == 0);
    CHECK(line.levels().empty());
    CHECK(line.runs_visual().empty());
    CHECK(line.char_offsets() == std::vector<size_t>{0});
}

TEST_CASE("BidiLine: pure LTR text is all at the base level (0)", "[bidi][levels]") {
    auto line = BidiLine::analyze("ABC", BaseDirection::LTR);
    CHECK(line.levels() == std::vector<uint8_t>{0, 0, 0});
}

TEST_CASE("BidiLine: pure RTL Hebrew text is all at level 1 under an LTR base",
         "[bidi][levels]") {
    auto line = BidiLine::analyze("אבג", BaseDirection::LTR); // אבג
    CHECK(line.levels() == std::vector<uint8_t>{1, 1, 1});
}

TEST_CASE("BidiLine: pure LTR text embedded in an RTL base paragraph is level 2",
         "[bidi][levels]") {
    // Odd (RTL) base level + strong L -> +1 (I2) = level 2.
    auto line = BidiLine::analyze("ABC", BaseDirection::RTL);
    CHECK(line.levels() == std::vector<uint8_t>{2, 2, 2});
}

TEST_CASE("BidiLine: pure RTL Hebrew text stays at the base level under an RTL base",
         "[bidi][levels]") {
    auto line = BidiLine::analyze("אבג", BaseDirection::RTL); // אבג
    CHECK(line.levels() == std::vector<uint8_t>{1, 1, 1});
}

TEST_CASE("BidiLine: reference case 'ABD אבג 123' resolves the documented level pattern",
         "[bidi][levels]") {
    // Hebrew letters are 2-byte UTF-8 (U+05D0..U+05D2); this is the canonical
    // mixed-script fixture from the design doc.
    auto line = BidiLine::analyze("ABD אבג 123", BaseDirection::LTR);
    REQUIRE(line.char_count() == 11);
    CHECK(line.levels() == std::vector<uint8_t>{0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2});
}

TEST_CASE("BidiLine: trailing whitespace resets to the base level (L1)", "[bidi][levels]") {
    // A trailing space after an RTL run must not visually look RTL: it has to
    // sit at the base (here LTR) level so it renders at the line's true end.
    auto line = BidiLine::analyze("אבג ", BaseDirection::LTR); // "אבג "
    REQUIRE(line.char_count() == 4);
    CHECK(line.levels()[0] == 1);
    CHECK(line.levels()[1] == 1);
    CHECK(line.levels()[2] == 1);
    CHECK(line.levels()[3] == 0); // trailing space reset to base level
}

TEST_CASE("BidiLine: all-whitespace text resolves without crashing", "[bidi][levels]") {
    auto line = BidiLine::analyze("   ", BaseDirection::LTR);
    CHECK(line.levels() == std::vector<uint8_t>{0, 0, 0});
}

TEST_CASE("BidiLine: a combining mark inherits the embedding level of its base character",
         "[bidi][levels]") {
    // U+05D1 (ב) + U+05B8 (QAMATS, a Hebrew point / NSM) — W1 makes the NSM
    // take the previous character's resolved type, so it lands on the same
    // (RTL) level as the letter it decorates.
    auto line = BidiLine::analyze("בָ", BaseDirection::LTR);
    REQUIRE(line.char_count() == 2);
    CHECK(line.levels()[0] == 1);
    CHECK(line.levels()[1] == 1);
}

// ── BidiLine::analyze: visual ordering ─────────────────────────────────────────

TEST_CASE("BidiLine: visual_to_logical reorders the reference case correctly",
         "[bidi][order]") {
    std::string text = "ABD אבג 123"; // "ABD אבג 123"
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    auto const &offs = line.char_offsets();

    std::string painted;
    for (auto logical_idx : line.visual_to_logical()) {
        painted += text.substr(offs[logical_idx], offs[logical_idx + 1] - offs[logical_idx]);
    }
    CHECK(painted == "ABD 123 גבא"); // "ABD 123 גבא"
}

TEST_CASE("BidiLine: pure RTL text is reversed in visual order", "[bidi][order]") {
    std::string text = "אבג"; // אבג
    auto line = BidiLine::analyze(text, BaseDirection::RTL);
    CHECK(line.visual_to_logical() == std::vector<size_t>{2, 1, 0});
}

TEST_CASE("BidiLine: pure LTR text is unchanged in visual order", "[bidi][order]") {
    auto line = BidiLine::analyze("ABC", BaseDirection::LTR);
    CHECK(line.visual_to_logical() == std::vector<size_t>{0, 1, 2});
}

TEST_CASE("BidiLine: logical_to_visual is the inverse of visual_to_logical",
         "[bidi][order]") {
    auto line = BidiLine::analyze("ABD אבג 123", BaseDirection::LTR);
    auto const &v2l = line.visual_to_logical();
    auto const &l2v = line.logical_to_visual();
    REQUIRE(v2l.size() == l2v.size());
    for (size_t k = 0; k < v2l.size(); ++k) {
        CHECK(l2v[v2l[k]] == k);
    }
    // Spot checks against the reference case: 'א' is logical index 4 and is
    // drawn last (visual slot 10); '1' is logical index 8 and is drawn at
    // visual slot 4 (right after the LTR prefix "ABD ").
    CHECK(l2v[4] == 10);
    CHECK(l2v[8] == 4);
}

// ── BidiLine::analyze: runs ─────────────────────────────────────────────────────

TEST_CASE("BidiLine: runs_visual splits the reference case into three left-to-right runs",
         "[bidi][runs]") {
    std::string text = "ABD אבג 123"; // "ABD אבג 123"
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    auto const &runs = line.runs_visual();

    REQUIRE(runs.size() == 3);

    CHECK(text.substr(runs[0].start, runs[0].length) == "ABD ");
    CHECK(runs[0].level == 0);
    CHECK_FALSE(runs[0].rtl());

    CHECK(text.substr(runs[1].start, runs[1].length) == "123");
    CHECK(runs[1].level == 2);
    CHECK_FALSE(runs[1].rtl());

    CHECK(text.substr(runs[2].start, runs[2].length) == "אבג "); // "אבג "
    CHECK(runs[2].level == 1);
    CHECK(runs[2].rtl());
}

TEST_CASE("BidiLine: pure LTR text is a single run", "[bidi][runs]") {
    auto line = BidiLine::analyze("Hello", BaseDirection::LTR);
    REQUIRE(line.runs_visual().size() == 1);
    CHECK(line.runs_visual()[0].level == 0);
}

TEST_CASE("BidiLine: pure RTL text is a single run", "[bidi][runs]") {
    auto line = BidiLine::analyze("אבג", BaseDirection::LTR);
    REQUIRE(line.runs_visual().size() == 1);
    CHECK(line.runs_visual()[0].level == 1);
    CHECK(line.runs_visual()[0].rtl());
}

// ── BidiLine::level_at ──────────────────────────────────────────────────────────

TEST_CASE("BidiLine: level_at reports the level of the character containing a byte",
         "[bidi][level_at]") {
    std::string text = "ABאב"; // "AB" + "אב"
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    auto const &offs = line.char_offsets();

    CHECK(line.level_at(offs[0]) == 0); // 'A'
    CHECK(line.level_at(offs[1]) == 0); // 'B'
    CHECK(line.level_at(offs[2]) == 1); // 'א'
    CHECK(line.level_at(offs[3]) == 1); // 'ב'
}

TEST_CASE("BidiLine: level_at at or past the end of the string returns the base level",
         "[bidi][level_at]") {
    std::string text = "אב"; // "אב"
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    CHECK(line.level_at(text.size()) == 0);     // end of string -> base level (LTR=0)
    CHECK(line.level_at(text.size() + 5) == 0); // past the end, same fallback
}

TEST_CASE("BidiLine: level_at on an empty line returns the base level", "[bidi][level_at]") {
    auto line = BidiLine::analyze("", BaseDirection::RTL);
    CHECK(line.level_at(0) == 1);
}

// ── BidiLine::char_offsets ──────────────────────────────────────────────────────

TEST_CASE("BidiLine: char_offsets are strictly increasing and span the whole string",
         "[bidi][offsets]") {
    std::string text = "Aא 1"; // "A" + "א" + " 1"
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    auto const &offs = line.char_offsets();

    REQUIRE(offs.size() == line.char_count() + 1);
    CHECK(offs.front() == 0);
    CHECK(offs.back() == text.size());
    for (size_t i = 1; i < offs.size(); ++i) {
        CHECK(offs[i] > offs[i - 1]);
    }
}

// ── BidiLine::analyze: Arabic gets the same treatment as Hebrew ───────────────
//
// Arabic letters classify as AL (not R) and only become R via W3, and a
// digit run immediately after them goes through W2 as AN rather than EN --
// a different code path than Hebrew's plain-R letters. These tests make
// sure that path is actually exercised, not just the Hebrew one.

TEST_CASE("BidiLine: pure RTL Arabic text is all at level 1 under an LTR base",
         "[bidi][levels]") {
    auto line = BidiLine::analyze("مرحبا", BaseDirection::LTR); // "marhaba" -- hello
    CHECK(line.levels() == std::vector<uint8_t>{1, 1, 1, 1, 1});
}

TEST_CASE("BidiLine: an Arabic combining mark inherits the embedding level of its base character",
         "[bidi][levels]") {
    // U+0628 (ب, BEH) + U+064B (FATHA, an Arabic combining mark / NSM).
    auto line = BidiLine::analyze("بَ", BaseDirection::LTR);
    REQUIRE(line.char_count() == 2);
    CHECK(line.levels()[0] == 1);
    CHECK(line.levels()[1] == 1);
}

TEST_CASE("BidiLine: Arabic reference case 'ABD مرحبا 123' resolves analogously to the Hebrew one",
         "[bidi][levels]") {
    // Unlike Hebrew, the digits here pass through W2 as AN (the last strong
    // type seen is AL, not R) rather than EN -- but I1 treats EN and AN
    // identically, so the final levels match the Hebrew fixture's shape.
    auto line = BidiLine::analyze("ABD مرحبا 123", BaseDirection::LTR);
    REQUIRE(line.char_count() == 13);
    CHECK(line.levels() == std::vector<uint8_t>{0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2});
}

TEST_CASE("BidiLine: visual_to_logical reorders the Arabic reference case correctly",
         "[bidi][order]") {
    std::string text = "ABD مرحبا 123";
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    auto const &offs = line.char_offsets();

    std::string painted;
    for (auto logical_idx : line.visual_to_logical()) {
        painted += text.substr(offs[logical_idx], offs[logical_idx + 1] - offs[logical_idx]);
    }
    CHECK(painted == "ABD 123 ابحرم"); // Arabic word reversed, as Hebrew was
}

// ── BidiLine::caret_rank — the "SALAM" interactive-typing scenario ────────────
//
// Walks through typing into an RTL-auto field one chunk at a time, exactly
// as a user filling in a LineInput would: first "سلام" ("salam", Arabic for
// peace), then appending "Hello", then "123", then a trailing ".". At each
// step the cursor sits at the logical end of the current text. caret_rank()
// reports how many characters are drawn to its visual left -- the ordering
// half of caret placement that TextLayout will later turn into a pixel x.

TEST_CASE("BidiLine: caret at the end of pure Arabic text is at the visual left edge",
         "[bidi][caret]") {
    std::string text = "سلام";
    auto base = detect_base_direction(text);
    REQUIRE(base == BaseDirection::RTL);
    auto line = BidiLine::analyze(text, base);
    REQUIRE(line.char_count() == 4);
    // Nothing is drawn to the caret's left: it sits at the line's leftmost
    // pixel even though it is the *logical* end of the string.
    CHECK(line.caret_rank(text.size()) == 0);
}

TEST_CASE("BidiLine: appending Latin text moves the caret from the left edge towards the middle",
         "[bidi][caret]") {
    std::string text = "سلامHello";
    auto base = detect_base_direction(text); // still RTL: the first strong char is still Arabic
    REQUIRE(base == BaseDirection::RTL);
    auto line = BidiLine::analyze(text, base);
    REQUIRE(line.char_count() == 9);
    // "Hello" (5 chars) is now drawn to the caret's left and "سلام" (4
    // chars) to its right: the caret sits past the midpoint, nowhere near
    // either edge of the line.
    CHECK(line.caret_rank(text.size()) == 5);
}

TEST_CASE("BidiLine: appending digits right after Latin text keeps the caret pinned to the same boundary",
         "[bidi][caret]") {
    // W7 reclassifies "123" as L because it directly follows Latin letters
    // with no intervening strong RTL character in between, so the digits
    // join the *same* run as "Hello" instead of starting a new one.
    std::string text = "سلامHello123";
    auto base = detect_base_direction(text);
    REQUIRE(base == BaseDirection::RTL);
    auto line = BidiLine::analyze(text, base);
    REQUIRE(line.char_count() == 12);
    CHECK(line.caret_rank(text.size()) == 8);
    // The same 4 Arabic characters are still to the caret's right: the
    // boundary with "سلام" has not moved, only the (now longer) Latin run
    // to its left has grown -- the caret stays on the left side of the
    // line, it just does not creep further left the way typing more
    // Arabic would.
    CHECK(line.char_count() - line.caret_rank(text.size()) == 4);
}

TEST_CASE("BidiLine: a trailing period flips the caret all the way to the visual left edge",
         "[bidi][caret]") {
    // "." has no strong direction of its own. With nothing after it, N2
    // resolves it towards the paragraph's base direction (RTL), landing it
    // at the same odd level as "سلام" even though it was typed last. L2
    // then reverses it past the whole embedded "Hello123" run, so the caret
    // jumps from the middle straight back to the far left -- a real,
    // well-known bidi edge case (trailing punctuation after an embedded
    // LTR run inside RTL text), not a quirk of this implementation.
    std::string text = "سلامHello123.";
    auto base = detect_base_direction(text);
    REQUIRE(base == BaseDirection::RTL);
    auto line = BidiLine::analyze(text, base);
    REQUIRE(line.char_count() == 13);
    CHECK(line.level_at(text.size() - 1) == 1); // the period itself
    CHECK(line.caret_rank(text.size()) == 0);
}

// ── Same "SALAM" scenario, but with the direction mode forced to LTR ──────────
//
// This is the explicit-LTR mode from the design (as opposed to RTL-Auto):
// the caller pins base direction instead of letting it follow the first
// strong character, so BaseDirection::LTR is passed in directly rather than
// going through detect_base_direction(). Same text, same steps as above --
// the contrast shows that forcing LTR is not just "the same thing flipped",
// it removes the surprising middle/jump behaviour entirely, because "Hello",
// "123" and "." all stay at the (now LTR) base level and simply ride along
// the end of the line like ordinary appended text. Only the leading "سلام"
// chunk is special: it is the one piece embedded at a foreign (odd) level.

TEST_CASE("BidiLine: forced LTR -- caret at the end of pure Arabic text is still at the visual left edge",
         "[bidi][caret]") {
    // With only the Arabic run on the line, level assignment lands on the
    // same odd level (1) whether the base is forced LTR or auto-detected
    // RTL -- a lone RTL run at either end of an otherwise-empty line looks
    // the same either way. The difference only shows up once LTR content is
    // appended (see below).
    std::string text = "سلام";
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    REQUIRE(line.char_count() == 4);
    CHECK(line.levels() == std::vector<uint8_t>{1, 1, 1, 1});
    CHECK(line.caret_rank(text.size()) == 0);
}

TEST_CASE("BidiLine: forced LTR -- appending Latin text snaps the caret straight to the right edge",
         "[bidi][caret]") {
    // Under RTL-Auto this same string put the caret at rank 5/9 (the
    // middle). Under forced LTR, "Hello" stays at the base level (0) since
    // it agrees with the (now LTR) paragraph direction, so it is not an
    // embedded run that floats to one side -- it is just the tail of an
    // ordinary LTR line. The caret goes straight to the end.
    std::string text = "سلامHello";
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    REQUIRE(line.char_count() == 9);
    CHECK(line.levels() == std::vector<uint8_t>{1, 1, 1, 1, 0, 0, 0, 0, 0});
    CHECK(line.caret_rank(text.size()) == 9); // all 9 characters are to its left
}

TEST_CASE("BidiLine: forced LTR -- appending digits keeps the caret pinned to the right edge",
         "[bidi][caret]") {
    std::string text = "سلامHello123";
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    REQUIRE(line.char_count() == 12);
    CHECK(line.caret_rank(text.size()) == 12); // still the absolute end of the line
}

TEST_CASE("BidiLine: forced LTR -- a trailing period also stays put at the right edge",
         "[bidi][caret]") {
    // Under RTL-Auto the trailing period flipped to the far *left* because
    // sos/eos was R, so an N2 tie resolved the neutral towards R. Here sos
    // is L (forced LTR), so the same tie resolves towards L instead: the
    // period joins "Hello123" at level 0 rather than jumping to "سلام"'s
    // level 1. Same character, opposite resolution, purely because of the
    // paragraph's base direction.
    std::string text = "سلامHello123.";
    auto line = BidiLine::analyze(text, BaseDirection::LTR);
    REQUIRE(line.char_count() == 13);
    CHECK(line.level_at(text.size() - 1) == 0); // the period -- 0, not 1 as under RTL-Auto
    CHECK(line.caret_rank(text.size()) == 13);  // still the absolute end of the line
}
