// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"

#include <cairo.h>
#include <fontconfig/fontconfig.h>
#include <harfbuzz/hb.h>
#include <string_view>
#include <vector>

namespace toolkit {

struct GlyphRun {
    std::vector<cairo_glyph_t> glyphs;
    double advance = 0.0;
    std::string font_name; // resolved font family for this run
};

struct ShapedText {
    std::vector<GlyphRun> runs;
    double total_advance = 0.0;
    std::vector<double> cursor_positions; // visual X for each logical codepoint boundary
};

class TextShaper {
  public:
    enum class TextDirection { Auto, LTR, RTL };

    TextShaper() = default;
    ~TextShaper();

    ShapedText shape(cairo_t *cr, std::string_view text, float font_size, FontFamily font,
                     bool bold = false, bool italic = false,
                     TextDirection direction = TextDirection::Auto);

    static void select_font_on_cr(cairo_t *cr, std::string_view family, float font_size, bool bold,
                                  bool italic);

  private:
    // Shaped text cache: avoid re-shaping when the same text is drawn again
    std::string cached_text_;
    std::vector<uint32_t> cached_text_utf32_;
    ShapedText cached_shaped_;
    float cached_text_size_ = -1;
    FontFamily cached_text_family_ = FontFamily::System;
    bool cached_text_bold_ = false;
    bool cached_text_italic_ = false;
    TextDirection cached_text_direction_ = TextDirection::Auto;
    bool cached_text_valid_ = false;

    // Per-run font cache: reuse hb_font across runs with the same
    // (resolved_family, size, bold, italic) to avoid expensive
    // FT_Face / hb_font creation.
    struct RunFontCache {
        std::string resolved_family;
        float font_size = -1;
        bool bold = false;
        bool italic = false;
        hb_font_t *hb_font = nullptr;
    } run_font_;

    // FcFontSort cache: FcFontSort(FC_FAMILY) is very expensive because it
    // iterates every installed font.  Cache the sorted set per unique family
    // name so we only pay the sort once and then do a cheap per-run coverage
    // walk on subsequent resolutions.
    struct FcSortCache {
        std::string preferred_family;
        FcFontSet *fs = nullptr;
    } fc_sort_cache_;

    auto get_font_set(std::string_view preferred_family) -> FcFontSet *;

    static auto resolve_font_for_codepoints(std::string_view preferred_family,
                                            FcFontSet *fs,
                                            std::vector<uint32_t> const &codepoints)
        -> std::string;

    static auto create_hb_font(cairo_t *cr, float font_size) -> hb_font_t *;
    static auto cairo_font_face_name(FontFamily f, cairo_t *cr) -> std::string;

    cairo_surface_t *temp_surf_ = nullptr;
    cairo_t *temp_cr_ = nullptr;
};

} // namespace toolkit
