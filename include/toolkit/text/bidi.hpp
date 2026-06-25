// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace toolkit::bidi {

enum class BaseDirection { LTR, RTL };

// P2/P3 of the Unicode Bidirectional Algorithm: the direction of the first
// strong (L/R/AL) character. Empty input, or input with no strong character,
// resolves to LTR. Kept as a free function so the "auto direction" policy can
// be swapped later without touching BidiLine.
BaseDirection detect_base_direction(std::string_view utf8);

// A maximal span of characters sharing one embedding level, in LOGICAL byte
// order (start/length always index a contiguous slice of the original text).
// This is the unit a TextShaper consumes; `rtl()` tells it which way to shape.
struct Run {
    size_t start = 0;
    size_t length = 0;
    uint8_t level = 0;

    bool rtl() const { return (level & 1u) != 0; }
};

// Result of resolving one line of text under a (deliberately scoped-down)
// subset of the Unicode Bidirectional Algorithm: a single paragraph, a single
// base embedding level, no explicit directional formatting characters or
// isolates (LRE/RLE/LRO/RLO/LRI/RLI/FSI/PDI are treated as neutral). That is
// enough for a single-line edit control; see docs/design/rtl-line-input.md.
//
// Everything here is keyed by BYTE offsets into the original UTF-8 string,
// and "character" means one decoded Unicode scalar value (not a grapheme
// cluster) — combining marks/clusters are a documented follow-up.
class BidiLine {
  public:
    static BidiLine analyze(std::string_view utf8, BaseDirection base);

    BaseDirection base() const { return base_; }
    size_t char_count() const { return levels_.size(); }

    // Embedding level per character, size char_count(). Even = LTR, odd = RTL.
    std::vector<uint8_t> const &levels() const { return levels_; }

    // Byte offset of each character boundary, size char_count() + 1 (the last
    // entry is the byte length of the string).
    std::vector<size_t> const &char_offsets() const { return offsets_; }

    // Embedding level of the character containing `byte`. Returns the base
    // level if `byte` is at or past the end of the string.
    uint8_t level_at(size_t byte) const;

    // Number of characters drawn strictly to the visual left of the caret
    // when it sits at logical byte position `byte_pos` (must be a valid
    // character boundary: 0, char_offsets().back(), or some other entry of
    // char_offsets()). This is the ordering half of caret placement: a
    // TextShaper later turns it into a pixel x by summing per-character
    // advances instead of just counting characters. Under a uniform
    // character width it doubles as a directly testable "how far across the
    // line is the caret" without needing any shaper at all.
    size_t caret_rank(size_t byte_pos) const;

    // Maximal same-level runs, ordered left-to-right for painting (UBA L2 at
    // run granularity). What TextLayout iterates to shape and place glyphs.
    std::vector<Run> const &runs_visual() const { return runs_visual_; }

    // Character-granularity permutations, mainly for tests and any future
    // visual-movement mode; TextLayout does not need these on its hot path.
    // visual_to_logical()[k] = logical char index drawn k-th from the left.
    std::vector<size_t> const &visual_to_logical() const { return visual_to_logical_; }
    // logical_to_visual()[i] = visual slot (0-based, left-to-right) of
    // logical char i. Inverse of visual_to_logical().
    std::vector<size_t> const &logical_to_visual() const { return logical_to_visual_; }

  private:
    BaseDirection base_ = BaseDirection::LTR;
    std::vector<uint8_t> levels_;
    std::vector<size_t> offsets_;
    std::vector<Run> runs_visual_;
    std::vector<size_t> visual_to_logical_;
    std::vector<size_t> logical_to_visual_;
};

} // namespace toolkit::bidi
