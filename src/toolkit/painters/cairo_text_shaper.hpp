// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"

#include <cairo.h>
#include <harfbuzz/hb.h>
#include <string_view>
#include <vector>

namespace toolkit {

struct GlyphRun {
    std::vector<cairo_glyph_t> glyphs;
    double advance = 0.0;
};

struct ShapedText {
    std::vector<GlyphRun> runs;
    double total_advance = 0.0;
    std::vector<double> cursor_positions; // visual X for each logical codepoint boundary
};

class TextShaper {
  public:
    TextShaper() = default;
    ~TextShaper();

    ShapedText shape(cairo_t *cr, std::string_view text, float font_size, FontFamily font,
                     bool bold = false, bool italic = false);

    void release_font();

  private:
    struct CachedFont {
        cairo_scaled_font_t *scaled_font = nullptr;
        hb_font_t *hb_font = nullptr;
    };

    CachedFont cached_font_;
    std::string cached_resolved_family_;
    FontFamily cached_family_ = FontFamily::System;
    float cached_size_ = -1;
    bool cached_bold_ = false;
    bool cached_italic_ = false;

    // Shaped text cache: avoid re-shaping when the same text is drawn again
    std::string cached_text_;
    std::vector<uint32_t> cached_text_utf32_;
    ShapedText cached_shaped_;
    float cached_text_size_ = -1;
    FontFamily cached_text_family_ = FontFamily::System;
    bool cached_text_bold_ = false;
    bool cached_text_italic_ = false;
    bool cached_text_valid_ = false;

    auto ensure_font(cairo_t *cr, float font_size, FontFamily font, bool bold, bool italic,
                     std::vector<uint32_t> const &codepoints) -> CachedFont;
    static void select_font_on_cr(cairo_t *cr, std::string_view family, float font_size, bool bold,
                                  bool italic);

    cairo_surface_t *temp_surf_ = nullptr;
    cairo_t *temp_cr_ = nullptr;
};

} // namespace toolkit
