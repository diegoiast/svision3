// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/painter.hpp"
#include "toolkit/text/bidi.hpp"
#include "toolkit/types.hpp"
#include <string_view>
#include <vector>

namespace toolkit::text {

// One shaped cluster, keyed to the LOGICAL byte offset (within the run
// passed to shape_run, i.e. 0 means the run's first byte) of its first
// character. v1 assumes one cluster per Unicode scalar value (no grapheme
// clustering / mark merging yet -- same documented scope limit as bidi::BidiLine).
struct ClusterAdvance {
    size_t byte_offset = 0;
    float advance = 0.0f;
};

// Shapes and draws one direction-uniform run of text. TextLayout only ever
// passes maximal same-level slices from bidi::BidiLine::runs_visual(), so
// implementations never have to deal with a level change mid-run.
class TextShaper {
  public:
    virtual ~TextShaper() = default;

    // `run_utf8` is in logical order. Returns one ClusterAdvance per cluster,
    // ordered VISUALLY left-to-right (for an rtl run that is the reverse of
    // logical order) so TextLayout can prefix-sum it directly into pixel x.
    virtual std::vector<ClusterAdvance> shape_run(std::string_view run_utf8, bool rtl,
                                                  float font_size, FontFamily font,
                                                  bool bold = false, bool italic = false) = 0;

    // Draws `run_utf8` starting at `origin` (left edge, baseline y), advancing
    // left-to-right -- the same geometry shape_run() reported.
    virtual void draw_run(Painter &painter, std::string_view run_utf8, bool rtl, Point origin,
                          Color const &color, float font_size, FontFamily font,
                          bool bold = false, bool italic = false) = 0;
};

// One visually-ordered, already-placed run, ready for painting.
struct VisualRun {
    size_t byte_start = 0;
    size_t byte_length = 0;
    bool rtl = false;
    float x = 0.0f;     // visual left edge, relative to the layout's origin
    float width = 0.0f;
};

// Pixel geometry for one line of text: combines bidi::BidiLine (ordering)
// with a TextShaper (glyph advances) so callers never mix the two. See
// docs/design/rtl-line-input.md section 5.
//
// All x values are relative to the layout's own origin (x=0 at the visual
// left edge of the line); callers translate into widget/window space.
// selection_rects() deliberately leaves y=0/height=0 -- vertical placement
// is a per-line-height decision the caller already owns (see
// BaseTheme::draw_line_input), not something a single-line layout should
// guess at.
class TextLayout {
  public:
    TextLayout(std::string_view utf8, bidi::BaseDirection base, TextShaper &shaper,
              float font_size, FontFamily font = FontFamily::System);

    float total_width() const { return total_width_; }

    // Visual x of the caret when it sits at logical byte position `byte_pos`
    // (must be a valid character boundary -- see bidi::BidiLine::char_offsets).
    float caret_x(size_t byte_pos) const;

    // Inverse of caret_x(): the logical byte position (a character boundary)
    // whose caret sits closest to visual x.
    size_t index_from_x(float x) const;

    // Visual rectangles covering the logical byte range [a, b). More than
    // one rect when the range crosses a direction boundary.
    std::vector<Rect> selection_rects(size_t a, size_t b) const;

    // Maximal same-level runs in visual (left-to-right paint) order.
    std::vector<VisualRun> const &runs() const { return runs_; }

    bidi::BidiLine const &bidi_line() const { return line_; }

  private:
    bidi::BidiLine line_;
    std::vector<VisualRun> runs_;
    std::vector<float> char_left_x_;  // size char_count(), screen-space left edge per character
    std::vector<float> char_right_x_; // screen-space right edge per character (>= left edge)
    float total_width_ = 0.0f;
};

} // namespace toolkit::text
