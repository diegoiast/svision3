// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include "toolkit/utf8.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace toolkit {

struct RasterizedText {
    std::vector<uint8_t> pixels; // RGBA, top-to-bottom, premultiplied
    int width = 0;               // pixel width
    int height = 0;              // pixel height
    float ascent = 0;            // font ascent in points
    float x_offset = 0;          // horizontal offset in points (for bearing)
};

class TextRasterizer {
  public:
    virtual ~TextRasterizer() = default;

    virtual RasterizedText rasterize(std::string_view text, float font_size, float scale,
                                     Color const &color, FontFamily font = FontFamily::System,
                                     bool bold = false, bool italic = false) = 0;

    // FIXME: those functions seem very similar. We could merge them
    // FIXME: Maybe cache the font metrics, instead of computing them?
    // FIXME: all platforms (win32, cairo) create a font and delete on each call
    //        It would be cool to tell the metric which font to use so they would
    //        cache.
    virtual Size measure(std::string_view text, float font_size,
                         FontFamily font = FontFamily::System) = 0;
    virtual Painter::FontMetrics metrics(float font_size, FontFamily font = FontFamily::System) = 0;

    // Visual X positions for each logical codepoint boundary (BiDi-aware).
    // direction can force a specific layout; Auto uses content-based detection.
    //
    // RTL convention (must match between all backends):
    //   Positions go from total_width (index 0, rightmost visual position)
    //   down to 0 (index n, leftmost visual position) — i.e. non-negative
    //   and decreasing.  LTR is the inverse:  0 … total_width, increasing.
    //   See win32_painter.cpp for the rationale.
    virtual std::vector<double>
    cursor_positions(std::string_view text, float font_size, FontFamily font = FontFamily::System,
                     Painter::TextDirection direction = Painter::TextDirection::Auto) {
        // Fallback: simple linear positions (LTR only)
        std::vector<double> pos(text.size() + 1, 0.0);
        // Naive LTR fallback - subclasses with BiDi override this
        return pos;
    }

    virtual void draw_text(Painter &p, std::string_view text, Point position, Color const &color,
                           float font_size, FontFamily font, Painter::TextOrientation orientation,
                           bool bold, bool italic) = 0;
};

class DummyRasterizer : public TextRasterizer {
  public:
    virtual RasterizedText rasterize(std::string_view t, float font_size, float scale,
                                     Color const & /*color*/, FontFamily f, bool /*bold*/ = false,
                                     bool /*italic*/ = false) override {
        auto r = RasterizedText{};
        auto m = measure(t, font_size, f);
        r.width = m.width;
        r.height = m.height;
        r.ascent = 0;
        return r;
    };

    static auto codepoint_count(std::string_view text) -> size_t {
        auto count = 0;
        auto i = 0;
        while (i < text.size()) {
            i = Utf8Iterator::next(text, i);
            count++;
        }
        return count;
    }

    virtual Size measure(std::string_view text, float font_size,
                         FontFamily font = FontFamily::System) override {
        auto y = 16.0f;
        return {8.0f * codepoint_count(text), y};
    };

    virtual Painter::FontMetrics metrics(float font_size,
                                         FontFamily font = FontFamily::System) override {
        return {0, 0, 0};
    };

    std::vector<double>
    cursor_positions(std::string_view text, float font_size, FontFamily font = FontFamily::System,
                     Painter::TextDirection = Painter::TextDirection::Auto) override {
        std::vector<double> pos(text.size() + 1, 0.0);
        auto x = 8.0f;
        size_t byte_pos = 0;
        int cp_idx = 0;
        while (byte_pos < text.size()) {
            auto next = Utf8Iterator::next(text, byte_pos);
            for (auto b = byte_pos; b < next; b++) {
                pos[b] = cp_idx * x;
            }
            pos[next] = (cp_idx + 1) * x;
            cp_idx++;
            byte_pos = next;
        }
        return pos;
    }

    virtual void draw_text(Painter &, std::string_view, Point, Color const &, float, FontFamily,
                           Painter::TextOrientation, bool, bool) override {}
};

} // namespace toolkit
