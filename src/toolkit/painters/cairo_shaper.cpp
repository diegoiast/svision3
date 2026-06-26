// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// !!! UNTESTED !!! Written without access to a Linux/Cairo/HarfBuzz/FreeType
// toolchain -- it has never been compiled, let alone run. See
// docs/design/cairo-shaper-handoff.md for implementation status, the
// reasoning behind the choices below, known gaps, and a verification
// checklist for whoever (human or LLM) picks this up on a real Linux box.

#ifdef TOOLKIT_HAS_CAIRO

#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/painters/cairo_shaper.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"

#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fontconfig.h>
#include <freetype/freetype.h>
#include <hb-ft.h>
#include <hb.h>

#include <cmath>
#include <vector>

namespace toolkit {

namespace {

// Decodes the single codepoint occupying utf8[pos, next) -- `next` must be
// `Utf8Iterator::next(utf8, pos)`, i.e. the caller already knows the byte
// length. Mirrors bidi.cpp's internal decode() (kept duplicated rather than
// shared, matching this codebase's existing per-file helper convention --
// see e.g. win32_shaper.cpp's own to_wide()/to_wide_run()).
char32_t decode_codepoint(std::string_view utf8, size_t pos, size_t next) {
    auto c0 = static_cast<unsigned char>(utf8[pos]);
    auto len = next - pos;
    if (len == 1) {
        return c0;
    }
    if (len == 2) {
        auto c1 = static_cast<unsigned char>(utf8[pos + 1]);
        return (static_cast<char32_t>(c0 & 0x1F) << 6) | (c1 & 0x3F);
    }
    if (len == 3) {
        auto c1 = static_cast<unsigned char>(utf8[pos + 1]);
        auto c2 = static_cast<unsigned char>(utf8[pos + 2]);
        return (static_cast<char32_t>(c0 & 0x0F) << 12) | (static_cast<char32_t>(c1 & 0x3F) << 6) |
               (c2 & 0x3F);
    }
    auto c1 = static_cast<unsigned char>(utf8[pos + 1]);
    auto c2 = static_cast<unsigned char>(utf8[pos + 2]);
    auto c3 = static_cast<unsigned char>(utf8[pos + 3]);
    return (static_cast<char32_t>(c0 & 0x07) << 18) | (static_cast<char32_t>(c1 & 0x3F) << 12) |
           (static_cast<char32_t>(c2 & 0x3F) << 6) | (c3 & 0x3F);
}

// A maximal slice of `run_utf8` that should be shaped as one HarfBuzz
// script. HarfBuzz applies script-specific shaping features (Arabic
// joining, Hebrew presentation forms, ...) per `hb_shape()` call, so a run
// mixing scripts at the SAME bidi level (e.g. Hebrew followed directly by
// Arabic, both RTL, no level change) still needs splitting here -- our own
// bidi::BidiLine only ever splits by *level*, never by script.
//
// Script lookup uses HarfBuzz's own Unicode data (hb_unicode_script()) --
// the full Unicode Scripts.txt table, already linked in via HarfBuzz, so no
// extra dependency (ICU, fribidi) is needed for this. COMMON/INHERITED/
// UNKNOWN characters (digits, punctuation, whitespace, combining marks)
// never start a new span; they join whichever strong-script span they sit
// in, the same way bidi neutrals join a surrounding strong direction.
struct ScriptSpan {
    size_t start = 0;
    size_t length = 0;
    hb_script_t script = HB_SCRIPT_COMMON;
};

std::vector<ScriptSpan> segment_by_script(std::string_view utf8) {
    std::vector<ScriptSpan> spans;
    if (utf8.empty()) {
        return spans;
    }

    auto *ufuncs = hb_unicode_funcs_get_default();
    size_t pos = 0;
    size_t seg_start = 0;
    auto seg_script = HB_SCRIPT_INVALID; // "no strong script seen yet in this span"

    while (pos < utf8.size()) {
        auto next = Utf8Iterator::next(utf8, pos);
        auto cp = decode_codepoint(utf8, pos, next);
        auto cp_script = hb_unicode_script(ufuncs, static_cast<hb_codepoint_t>(cp));
        auto is_common = cp_script == HB_SCRIPT_COMMON || cp_script == HB_SCRIPT_INHERITED ||
                         cp_script == HB_SCRIPT_UNKNOWN;
        if (!is_common) {
            if (seg_script == HB_SCRIPT_INVALID) {
                seg_script = cp_script;
            } else if (cp_script != seg_script) {
                spans.push_back({seg_start, pos - seg_start, seg_script});
                seg_start = pos;
                seg_script = cp_script;
            }
        }
        pos = next;
    }
    spans.push_back(
        {seg_start, utf8.size() - seg_start, seg_script == HB_SCRIPT_INVALID ? HB_SCRIPT_COMMON : seg_script});
    return spans;
}

std::string family_for(FontFamily family) {
    auto const &fonts = Theme::current().palette.fonts;
    return family == FontFamily::Monospace ? fonts.monospace : fonts.system;
}

// Resolves a CSS-style family name ("sans-serif", "monospace", or a real
// family name) to a concrete font file path + face index via fontconfig.
// Assumes fontconfig has already been initialized somewhere at the
// application/platform layer (see handoff doc -- this was NOT verified).
bool resolve_font_file(std::string const &family, std::string &path_out, int &index_out) {
    auto *pattern = FcNameParse(reinterpret_cast<FcChar8 const *>(family.c_str()));
    if (!pattern) {
        return false;
    }
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result{};
    auto *match = FcFontMatch(nullptr, pattern, &result);
    FcPatternDestroy(pattern);
    if (!match) {
        return false;
    }

    FcChar8 *file = nullptr;
    auto ok = FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch;
    if (ok) {
        path_out = reinterpret_cast<char const *>(file);
        int index = 0;
        FcPatternGetInteger(match, FC_INDEX, 0, &index);
        index_out = index;
    }
    FcPatternDestroy(match);
    return ok;
}

// One shaped span: HarfBuzz's own glyph-info/-position arrays, copied out
// of the (destroyed-before-return) hb_buffer_t. `infos[i].cluster` is the
// UTF-8 byte offset *within the span* (0 = span's first byte) that glyph i
// maps back to; `infos[i].codepoint` is, post-shape, the FreeType glyph
// index in `font`'s face -- directly usable as a cairo_glyph_t::index
// against a cairo_font_face_t built from that SAME FT_Face (see draw_run).
struct ShapeResult {
    std::vector<hb_glyph_info_t> infos;
    std::vector<hb_glyph_position_t> positions;
};

ShapeResult shape_one_span(hb_font_t *font, std::string_view span_utf8, hb_script_t script, bool rtl) {
    ShapeResult out;
    auto *buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, span_utf8.data(), static_cast<int>(span_utf8.size()), 0, -1);
    hb_buffer_set_direction(buf, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, script);
    hb_buffer_set_language(buf, hb_language_get_default());
    hb_shape(font, buf, nullptr, 0);

    unsigned int count = 0;
    auto *infos = hb_buffer_get_glyph_infos(buf, &count);
    auto *positions = hb_buffer_get_glyph_positions(buf, &count);
    out.infos.assign(infos, infos + count);
    out.positions.assign(positions, positions + count);
    hb_buffer_destroy(buf);
    return out;
}

// Collapses a shaped span's glyphs into one ClusterAdvance per distinct
// `cluster` value (summing the advances of every glyph that shares it),
// mirroring win32_shaper.cpp's item_to_scalars() grouping. `span_offset` is
// added so the result is relative to the RUN, not the span. Per
// ClusterAdvance's v1 contract, this approximates ligatures/marks as a
// single wider "cluster" rather than reporting per-character sub-widths --
// same documented limitation as the Win32 backend.
std::vector<text::ClusterAdvance> advances_from_shape(ShapeResult const &shaped, size_t span_offset) {
    std::vector<text::ClusterAdvance> out;
    auto const n = shaped.infos.size();
    size_t i = 0;
    while (i < n) {
        auto cluster = shaped.infos[i].cluster;
        auto advance = 0.0f;
        size_t j = i;
        while (j < n && shaped.infos[j].cluster == cluster) {
            advance += static_cast<float>(shaped.positions[j].x_advance) / 64.0f;
            ++j;
        }
        out.push_back({span_offset + cluster, advance});
        i = j;
    }
    return out;
}

} // namespace

struct CairoShaper::Impl {
    FT_Library ft_library = nullptr;

    Impl() {
        if (FT_Init_FreeType(&ft_library) != 0) {
            ft_library = nullptr;
        }
    }

    ~Impl() {
        if (ft_library) {
            FT_Done_FreeType(ft_library);
        }
    }

    // Caller must FT_Done_Face() the result. A fresh face is loaded per
    // call (no caching) -- same simplicity-over-micro-optimization choice
    // win32_shaper.cpp makes with CreateFontW; see handoff doc for the
    // follow-up note on caching by (family, pixel size).
    FT_Face load_face(FontFamily family, float font_size) {
        if (!ft_library) {
            return nullptr;
        }
        std::string path;
        int index = 0;
        if (!resolve_font_file(family_for(family), path, index)) {
            return nullptr;
        }
        FT_Face face = nullptr;
        if (FT_New_Face(ft_library, path.c_str(), index, &face) != 0) {
            return nullptr;
        }
        FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::lround(font_size)));
        return face;
    }
};

CairoShaper::CairoShaper() : impl_(std::make_unique<Impl>()) {}

CairoShaper::~CairoShaper() = default;

std::vector<text::ClusterAdvance> CairoShaper::shape_run(std::string_view run_utf8, bool rtl,
                                                         float font_size, FontFamily font) {
    std::vector<text::ClusterAdvance> result;
    if (run_utf8.empty()) {
        return result;
    }

    FT_Face face = impl_->load_face(font, font_size);
    if (!face) {
        return result;
    }
    auto *hb_font = hb_ft_font_create_referenced(face);

    auto spans = segment_by_script(run_utf8);
    std::vector<std::vector<text::ClusterAdvance>> per_span;
    per_span.reserve(spans.size());
    for (auto const &span : spans) {
        auto shaped = shape_one_span(hb_font, run_utf8.substr(span.start, span.length), span.script, rtl);
        per_span.push_back(advances_from_shape(shaped, span.start));
    }

    // Spans were sliced off `run_utf8` in LOGICAL order. Each span's own
    // glyph order is already final visual order (HarfBuzz's own guarantee
    // for the direction set on its buffer), but for an RTL run the spans
    // THEMSELVES still need reversing relative to each other -- the
    // last-typed span renders leftmost -- mirroring win32_shaper.cpp's
    // per-item reversal one level up (script spans here play the role
    // Uniscribe's ScriptItemize items play there).
    if (!rtl) {
        for (auto &entries : per_span) {
            result.insert(result.end(), entries.begin(), entries.end());
        }
    } else {
        for (auto it = per_span.rbegin(); it != per_span.rend(); ++it) {
            result.insert(result.end(), it->begin(), it->end());
        }
    }

    hb_font_destroy(hb_font);
    FT_Done_Face(face);
    return result;
}

void CairoShaper::draw_run(Painter &painter, std::string_view run_utf8, bool rtl, Point origin,
                          Color const &color, float font_size, FontFamily font) {
    if (run_utf8.empty()) {
        return;
    }

    auto *cairo_painter = dynamic_cast<CairoPainter *>(&painter);
    if (!cairo_painter) {
        return; // v1: only the Cairo backend can interop with HarfBuzz/FreeType glyph indices here
    }
    auto *cr = cairo_painter->cairo();

    FT_Face face = impl_->load_face(font, font_size);
    if (!face) {
        return;
    }
    auto *hb_font = hb_ft_font_create_referenced(face);

    // cairo_glyph_t::index is a glyph ID in the FreeType face's own glyph
    // table -- the SAME space HarfBuzz's post-shape codepoint values use
    // when shaped against an hb_font_t built from this same FT_Face (see
    // ShapeResult's doc comment above). Building the cairo_font_face_t
    // from this exact `face` guarantees the indices line up.
    auto *cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
    cairo_set_font_face(cr, cairo_face);
    cairo_set_font_size(cr, font_size);
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

    // Cairo's CTM (set up by the X11/Wayland backend before constructing
    // this CairoPainter) already bakes in the DPI scale -- unlike
    // win32_shaper.cpp, there is no separate scale factor to multiply
    // font_size by here; see handoff doc.
    auto x = static_cast<double>(origin.x);
    auto y = static_cast<double>(origin.y);

    auto spans = segment_by_script(run_utf8);
    auto draw_one_span = [&](ScriptSpan const &span) {
        auto shaped = shape_one_span(hb_font, run_utf8.substr(span.start, span.length), span.script, rtl);
        auto const count = shaped.infos.size();
        if (count == 0) {
            return;
        }
        std::vector<cairo_glyph_t> glyphs(count);
        for (size_t i = 0; i < count; ++i) {
            glyphs[i].index = shaped.infos[i].codepoint;
            glyphs[i].x = x + static_cast<double>(shaped.positions[i].x_offset) / 64.0;
            // HarfBuzz's y axis is font/PostScript-style (up is positive);
            // Cairo's is screen-style (down is positive) -- NEGATE both the
            // per-glyph y_offset and the y_advance. Sign not verified on a
            // real renderer; see handoff doc.
            glyphs[i].y = y - static_cast<double>(shaped.positions[i].y_offset) / 64.0;
            x += static_cast<double>(shaped.positions[i].x_advance) / 64.0;
            y -= static_cast<double>(shaped.positions[i].y_advance) / 64.0;
        }
        cairo_show_glyphs(cr, glyphs.data(), static_cast<int>(count));
    };

    if (!rtl) {
        for (auto const &span : spans) {
            draw_one_span(span);
        }
    } else {
        for (auto it = spans.rbegin(); it != spans.rend(); ++it) {
            draw_one_span(*it);
        }
    }

    cairo_font_face_destroy(cairo_face);
    hb_font_destroy(hb_font);
    FT_Done_Face(face);
}

} // namespace toolkit

#endif // TOOLKIT_HAS_CAIRO
