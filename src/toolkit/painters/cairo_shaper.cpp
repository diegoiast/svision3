// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#ifdef TOOLKIT_HAS_CAIRO

#include "toolkit/painters/cairo_shaper.hpp"
#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"

#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fontconfig.h>
#include <freetype/freetype.h>
#include <hb-ft.h>
#include <hb-ot.h>
#include <hb.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <map>
#include <string>
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

    auto ufuncs = hb_unicode_funcs_get_default();
    auto pos = size_t{0};
    auto seg_start = size_t{0};
    auto seg_script = HB_SCRIPT_INVALID;

    while (pos < utf8.size()) {
        auto next = Utf8Iterator::next(utf8, pos);
        auto cp = decode_codepoint(utf8, pos, next);
        auto cp_script = hb_unicode_script(ufuncs, static_cast<hb_codepoint_t>(cp));
        auto is_weak = cp_script == HB_SCRIPT_COMMON || cp_script == HB_SCRIPT_INHERITED ||
                       cp_script == HB_SCRIPT_UNKNOWN;

        if (is_weak) {
            // A weak character following a strong script closes that span and
            // opens a COMMON run. The COMMON run will absorb the next strong
            // script (so "★ français" becomes a LATIN span, not a COMMON one).
            if (seg_script != HB_SCRIPT_INVALID && seg_script != HB_SCRIPT_COMMON) {
                spans.push_back({seg_start, pos - seg_start, seg_script});
                seg_start = pos;
                seg_script = HB_SCRIPT_COMMON;
            }
            // Already COMMON or INVALID: just continue accumulating.
        } else {
            if (seg_script == HB_SCRIPT_INVALID || seg_script == HB_SCRIPT_COMMON) {
                // No strong script yet, or inside a COMMON run: absorb this
                // script so the COMMON prefix joins this strong-script span.
                seg_script = cp_script;
            } else if (cp_script != seg_script) {
                // Different strong script: close current span, start new one.
                spans.push_back({seg_start, pos - seg_start, seg_script});
                seg_start = pos;
                seg_script = cp_script;
            }
        }
        pos = next;
    }

    if (seg_start < utf8.size()) {
        auto final_script = (seg_script == HB_SCRIPT_INVALID) ? HB_SCRIPT_COMMON : seg_script;
        spans.push_back({seg_start, utf8.size() - seg_start, final_script});
    }
    return spans;
}

std::string family_for(FontFamily family) {
    auto const &fonts = Theme::current().palette.fonts;
    return family == FontFamily::Monospace ? fonts.monospace : fonts.system;
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

ShapeResult shape_one_span(hb_font_t *font, std::string_view span_utf8, hb_script_t script,
                           bool rtl) {
    ShapeResult out;
    auto buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, span_utf8.data(), static_cast<int>(span_utf8.size()), 0, -1);
    hb_buffer_set_direction(buf, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, script);
    hb_buffer_set_language(buf, hb_language_get_default());
    hb_shape(font, buf, nullptr, 0);

    auto count = 0U;
    auto infos = hb_buffer_get_glyph_infos(buf, &count);
    auto positions = hb_buffer_get_glyph_positions(buf, &count);
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
std::vector<text::ClusterAdvance> advances_from_shape(ShapeResult const &shaped,
                                                      size_t span_offset) {
    std::vector<text::ClusterAdvance> out;
    auto const n = shaped.infos.size();
    auto i = 0;
    while (i < n) {
        auto cluster = shaped.infos[i].cluster;
        auto advance = 0.0f;
        auto j = i;
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

// One loaded font, keyed by the actual file+face+size rather than the logical
// family name. Different scripts that resolve to the same font file share a
// single FaceEntry, keeping resource use proportional to distinct physical
// fonts rather than distinct (family,script) pairs.
//
// Two separate FT_Face instances are required: hb_ft_face_create_referenced
// installs FreeType user-data hooks that conflict with Cairo's own hooks when
// they share an instance (Cairo asserts at exit). ft_face_hb is HarfBuzz's
// exclusive copy; ft_face_cairo is Cairo's exclusive copy from the same file.
//
// hb_ot_font_set_funcs + hb_font_set_scale make HarfBuzz read metrics from
// OpenType tables directly, so Cairo's internal FT_Set_Char_Size calls during
// cairo_show_glyphs never corrupt HarfBuzz's size state.
struct FaceEntry {
    FT_Face ft_face_hb = nullptr;
    hb_font_t *hb_font = nullptr;
    FT_Face ft_face_cairo = nullptr;
    cairo_font_face_t *cairo_face = nullptr;
};

struct FaceKey {
    std::string path;
    int face_index = 0;
    int pixel_size = 0;

    bool operator<(FaceKey const &o) const {
        if (path != o.path) {
            return path < o.path;
        }
        if (face_index != o.face_index) {
            return face_index < o.face_index;
        }
        return pixel_size < o.pixel_size;
    }
};

struct CairoShaper::Impl {
    FT_Library ft_library = nullptr;
    std::map<FaceKey, FaceEntry> face_cache;
    // FcFontSort result per family, lazily filled
    std::map<std::string, FcFontSet *> fc_sets; 

    Impl() {
        if (FT_Init_FreeType(&ft_library) != 0) {
            ft_library = nullptr;
        }
    }

    ~Impl() {
        for (auto &[key, e] : face_cache) {
            if (e.cairo_face) {
                cairo_font_face_destroy(e.cairo_face);
            }
            if (e.ft_face_cairo) {
                FT_Done_Face(e.ft_face_cairo);
            }
            if (e.hb_font) {
                hb_font_destroy(e.hb_font);
            }
            if (e.ft_face_hb) {
                FT_Done_Face(e.ft_face_hb);
            }
        }
        for (auto &[fam, fs] : fc_sets) {
            if (fs) {
                FcFontSetDestroy(fs);
            }
        }
        if (ft_library) {
            FT_Done_FreeType(ft_library);
        }
    }

    // Returns the FcFontSort set for `family`, caching it so FcFontSort
    // (which scans the whole system font database) runs at most once per family.
    FcFontSet *fc_set_for(std::string const &family) {
        auto it = fc_sets.find(family);
        if (it != fc_sets.end()) {
            return it->second;
        }

        auto *pat = FcPatternCreate();
        FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<FcChar8 const *>(family.c_str()));
        FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
        FcConfigSubstitute(nullptr, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);
        auto result = FcResult{};
        FcFontSet *fs = FcFontSort(nullptr, pat, FcTrue, nullptr, &result);
        FcPatternDestroy(pat);

        fc_sets[family] = fs;
        return fs;
    }

    // Walk the FcFontSort set for `family` and return the first font whose
    // FC_CHARSET covers all non-whitespace codepoints in `span_utf8`.
    // Falls back to the first font in the set if no perfect match is found.
    bool find_font_file(std::string const &family, std::string_view span_utf8,
                        std::string &path_out, int &index_out) {
        auto fs = fc_set_for(family);
        if (!fs || fs->nfont == 0) {
            return false;
        }

        auto needed = FcCharSetCreate();
        auto  pos = 0;
        while (pos < span_utf8.size()) {
            auto next = Utf8Iterator::next(span_utf8, pos);
            auto cp = decode_codepoint(span_utf8, pos, next);
            if (cp > 0x20u) {
                FcCharSetAddChar(needed, cp);
            }
            pos = next;
        }

        std::string fallback_path;
        auto fallback_index = 0;
        auto found = false;
        for (auto i = 0; i < fs->nfont && !found; ++i) {
            FcChar8 *file = nullptr;
            if (FcPatternGetString(fs->fonts[i], FC_FILE, 0, &file) != FcResultMatch) {
                continue;
            }

            if (fallback_path.empty()) {
                fallback_path = reinterpret_cast<char const *>(file);
                FcPatternGetInteger(fs->fonts[i], FC_INDEX, 0, &fallback_index);
            }

            FcCharSet *fcs = nullptr;
            if (FcPatternGetCharSet(fs->fonts[i], FC_CHARSET, 0, &fcs) != FcResultMatch || !fcs) {
                continue;
            }
            if (FcCharSetIsSubset(needed, fcs)) {
                path_out = reinterpret_cast<char const *>(file);
                FcPatternGetInteger(fs->fonts[i], FC_INDEX, 0, &index_out);
                found = true;
            }
        }

        FcCharSetDestroy(needed);
        if (!found && !fallback_path.empty()) {
            path_out = fallback_path;
            index_out = fallback_index;
            found = true;
        }
        return found;
    }

    // Returns a cached FaceEntry for the font that covers `span_utf8` at the
    // requested family and size, loading it on first use. Returns nullptr on
    // any failure (FreeType, Cairo, or fontconfig).
    FaceEntry const *get(FontFamily family, float font_size, std::string_view span_utf8) {
        if (!ft_library) {
            return nullptr;
        }

        std::string path;
        auto face_index = 0;
        if (!find_font_file(family_for(family), span_utf8, path, face_index)) {
            return nullptr;
        }

        FaceKey key;
        key.path = path;
        key.face_index = face_index;
        key.pixel_size = static_cast<int>(std::lround(font_size));

        auto it = face_cache.find(key);
        if (it != face_cache.end()) {
            return &it->second;
        }

        FT_Face face_hb = nullptr;
        if (FT_New_Face(ft_library, path.c_str(), face_index, &face_hb) != 0) {
            return nullptr;
        }
        auto hb_face_obj = hb_ft_face_create_referenced(face_hb);
        auto hb = hb_font_create(hb_face_obj);
        hb_ot_font_set_funcs(hb);
        hb_font_set_scale(hb, key.pixel_size * 64, key.pixel_size * 64);
        hb_face_destroy(hb_face_obj);

        FT_Face face_cairo = nullptr;
        if (FT_New_Face(ft_library, path.c_str(), face_index, &face_cairo) != 0) {
            hb_font_destroy(hb);
            FT_Done_Face(face_hb);
            return nullptr;
        }
        auto cairo_face = cairo_ft_font_face_create_for_ft_face(face_cairo, 0);
        if (cairo_font_face_status(cairo_face) != CAIRO_STATUS_SUCCESS) {
            cairo_font_face_destroy(cairo_face);
            FT_Done_Face(face_cairo);
            hb_font_destroy(hb);
            FT_Done_Face(face_hb);
            return nullptr;
        }

        FaceEntry e;
        e.ft_face_hb = face_hb;
        e.hb_font = hb;
        e.ft_face_cairo = face_cairo;
        e.cairo_face = cairo_face;
        auto [inserted, ok] = face_cache.emplace(key, e);
        return &inserted->second;
    }
};

CairoShaper::CairoShaper() : impl_(std::make_unique<Impl>()) {}

CairoShaper::~CairoShaper() = default;

void CairoShaper::release_fonts() {
    if (!impl_) {
        return;
    }
    for (auto &[key, e] : impl_->face_cache) {
        if (e.cairo_face) {
            cairo_font_face_destroy(e.cairo_face);
            e.cairo_face = nullptr;
        }
        if (e.ft_face_cairo) {
            FT_Done_Face(e.ft_face_cairo);
            e.ft_face_cairo = nullptr;
        }
        if (e.hb_font) {
            hb_font_destroy(e.hb_font);
            e.hb_font = nullptr;
        }
        if (e.ft_face_hb) {
            FT_Done_Face(e.ft_face_hb);
            e.ft_face_hb = nullptr;
        }
    }
    impl_->face_cache.clear();
}

std::vector<text::ClusterAdvance> CairoShaper::shape_run(std::string_view run_utf8, bool rtl,
                                                         float font_size, FontFamily font) {
    std::vector<text::ClusterAdvance> result;
    if (run_utf8.empty()) {
        return result;
    }

    auto spans = segment_by_script(run_utf8);
    std::vector<std::vector<text::ClusterAdvance>> per_span;
    per_span.reserve(spans.size());

    for (auto const &span : spans) {
        auto span_text = run_utf8.substr(span.start, span.length);
        auto entry = impl_->get(font, font_size, span_text);
        if (!entry) {
            per_span.push_back({});
            continue;
        }
        auto shaped = shape_one_span(entry->hb_font, run_utf8.substr(span.start, span.length),
                                     span.script, rtl);
        per_span.push_back(advances_from_shape(shaped, span.start));
    }

    if (!rtl) {
        for (auto &entries : per_span) {
            result.insert(result.end(), entries.begin(), entries.end());
        }
    } else {
        for (auto it = per_span.rbegin(); it != per_span.rend(); ++it) {
            result.insert(result.end(), it->begin(), it->end());
        }
    }
    return result;
}

void CairoShaper::draw_run(Painter &painter, std::string_view run_utf8, bool rtl, Point origin,
                           Color const &color, float font_size, FontFamily font) {
    if (run_utf8.empty()) {
        return;
    }

    auto cairo_painter = dynamic_cast<CairoPainter *>(&painter);
    if (!cairo_painter) {
        spdlog::debug("ERROR: using the cairo shaper without a cairo painter");
        return;
    }
    
    auto *cr = cairo_painter->cairo();
    cairo_save(cr);

    auto x = static_cast<double>(origin.x);
    auto y = static_cast<double>(origin.y);
    auto spans = segment_by_script(run_utf8);

    auto draw_one_span = [&](ScriptSpan const &span) {
        auto span_text = run_utf8.substr(span.start, span.length);
        auto *entry = impl_->get(font, font_size, span_text);
        if (!entry) {
            return;
        }

        // cairo_face is owned by the cache; don't destroy it here.
        // Using the same pointer every frame lets Cairo reuse its
        // cairo_scaled_font_t and glyph raster cache.
        cairo_set_font_face(cr, entry->cairo_face);
        cairo_set_font_size(cr, font_size);
        cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

        auto shaped = shape_one_span(entry->hb_font, run_utf8.substr(span.start, span.length),
                                     span.script, rtl);
        auto const count = shaped.infos.size();
        if (count > 0) {
            std::vector<cairo_glyph_t> glyphs(count);
            for (auto i = 0; i < count; ++i) {
                glyphs[i].index = shaped.infos[i].codepoint;
                glyphs[i].x = x + static_cast<double>(shaped.positions[i].x_offset) / 64.0;
                // HarfBuzz y is PostScript-style (up+); Cairo is screen-style (down+).
                glyphs[i].y = y - static_cast<double>(shaped.positions[i].y_offset) / 64.0;
                x += static_cast<double>(shaped.positions[i].x_advance) / 64.0;
                y -= static_cast<double>(shaped.positions[i].y_advance) / 64.0;
            }
            cairo_show_glyphs(cr, glyphs.data(), static_cast<int>(count));
        }
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

    cairo_restore(cr);
}

} // namespace toolkit

#endif // TOOLKIT_HAS_CAIRO
