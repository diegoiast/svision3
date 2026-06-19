// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#ifdef TOOLKIT_HAS_TEXT_SHAPER

#include "toolkit/painters/cairo_text_shaper.hpp"
#include "toolkit/theme.hpp"

#include <algorithm>
#include <cairo-ft.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fontconfig/fontconfig.h>
#include <fribidi.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb-ot.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace toolkit {

// ── FcInit guard (once per process) ─────────────────────────────────────────

namespace {

auto fc_ensure_init() -> bool {
    static bool ok = FcInit();
    return ok;
}

} // namespace

// ── UTF-8 conversion helpers ────────────────────────────────────────────────

static auto utf8_to_utf32(std::string_view utf8) -> std::vector<uint32_t> {
    std::vector<uint32_t> result;
    result.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size();) {
        auto c = static_cast<unsigned char>(utf8[i++]);
        uint32_t cp;
        if (c < 0x80) {
            cp = c;
        } else if (c < 0xE0) {
            cp = c & 0x1F;
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i++]) & 0x3F);
        } else if (c < 0xF0) {
            cp = c & 0x0F;
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i++]) & 0x3F);
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i++]) & 0x3F);
        } else {
            cp = c & 0x07;
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i++]) & 0x3F);
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i++]) & 0x3F);
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i++]) & 0x3F);
        }
        result.push_back(cp);
    }
    return result;
}

static auto utf32_to_utf8(std::vector<uint32_t> const &utf32) -> std::string {
    std::string result;
    for (auto cp : utf32) {
        if (cp < 0x80) {
            result += static_cast<char>(cp);
        } else if (cp < 0x800) {
            result += static_cast<char>(0xC0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            result += static_cast<char>(0xE0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (cp >> 18));
            result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return result;
}

// ── Font resolution via fontconfig ──────────────────────────────────────────

// Heuristic: check if a concrete font name belongs to the requested generic
// family.  This only rejects fonts whose name clearly indicates a different
// generic class (e.g. a "Serif" font when we asked for "sans-serif").
static auto generic_category_allows(std::string_view preferred_generic,
                                    std::string_view resolved_name) -> bool {
    if (preferred_generic == "sans-serif") {
        if (resolved_name.find("Serif") != std::string_view::npos) {
            return false;
        }
    } else if (preferred_generic == "serif") {
        if (resolved_name.find("Sans") != std::string_view::npos) {
            return false;
        }
    }
    return true;
}

static auto resolve_font_for_codepoints(std::string_view preferred_family,
                                        std::vector<uint32_t> const &codepoints) -> std::string {
    fc_ensure_init();

    if (codepoints.empty()) {
        return std::string(preferred_family);
    }

    // Build a charset from the requested codepoints (used for coverage
    // filtering, NOT passed to FcFontSort so that sort order is purely
    // based on family match quality).
    FcCharSet *cs = FcCharSetCreate();
    for (auto cp : codepoints) {
        FcCharSetAddChar(cs, cp);
    }

    // Sort fonts by family match — no charset constraint, so generic
    // families like "sans-serif" correctly prefer sans-serif fonts.
    FcPattern *pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<FcChar8 const *>(preferred_family.data()));
    FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(nullptr, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result;
    FcFontSet *fs = FcFontSort(nullptr, pat, FcTrue, nullptr, &result);
    FcPatternDestroy(pat);

    std::string family(preferred_family);
    if (fs) {
        // Single pass: track the best category-matched font AND the best
        // any-category font that cover the codepoints.  Prefer category match,
        // fall back to any covering font.
        std::string best_category;
        std::string best_any;
        FcCharSet *cover = FcCharSetCopy(cs);

        for (int i = 0; i < fs->nfont; i++) {
            FcCharSet *fcs = nullptr;
            if (FcPatternGetCharSet(fs->fonts[i], FC_CHARSET, 0, &fcs) != FcResultMatch || !fcs) {
                continue;
            }
            if (!FcCharSetIsSubset(cover, fcs)) {
                continue;
            }
            FcChar8 *fam = nullptr;
            if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &fam) != FcResultMatch) {
                continue;
            }
            auto name = reinterpret_cast<char *>(fam);
            if (generic_category_allows(preferred_family, name)) {
                best_category = name;
                break;
            }
            if (best_any.empty()) {
                best_any = name;
            }
        }

        if (!best_category.empty()) {
            family = best_category;
        } else if (!best_any.empty()) {
            family = best_any;
        }

        FcCharSetDestroy(cover);
        FcFontSetDestroy(fs);
    }
    FcCharSetDestroy(cs);
    return family;
}

// ── Font face name (preferred family, no coverage check) ────────────────────

static auto probe_monospace_font() -> std::string {
    // Use an isolated temporary cairo context so font probing has zero
    // side‑effect on any caller context (internal cairo caches, font
    // options, CTM, etc.).
    auto *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto *cr = cairo_create(surf);

    static const char *candidates[] = {
        "DejaVu Sans Mono", "Liberation Mono", "Courier New", "Noto Mono", "Hack",
        "Ubuntu Mono",      "Courier",         nullptr};
    cairo_set_font_size(cr, 12.0);
    std::string found;
    for (auto **name = candidates; *name; ++name) {
        cairo_select_font_face(cr, *name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_text_extents_t ti, tm;
        cairo_text_extents(cr, "i", &ti);
        cairo_text_extents(cr, "m", &tm);
        if (std::abs(ti.x_advance - tm.x_advance) < 0.1) {
            found = *name;
            break;
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return found.empty() ? "monospace" : found;
}

static auto cairo_font_face_name(FontFamily f, cairo_t * /*cr*/) -> std::string {
    if (f == FontFamily::Monospace) {
        return probe_monospace_font();
    }
    return Theme::current().palette.fonts.system;
}

// ── Bidi run helpers ────────────────────────────────────────────────────────

struct BidiRun {
    int logical_start; // index into the UTF-32 array (inclusive)
    int logical_end;   // index into the UTF-32 array (exclusive)
    FriBidiLevel level;

    auto is_rtl() const -> bool { return level % 2 == 1; }
};

static auto reorder_runs(std::vector<BidiRun> runs) -> std::vector<BidiRun> {
    if (runs.empty()) {
        return {};
    }

    auto max_level = FriBidiLevel{0};
    for (auto &r : runs) {
        if (r.level > max_level) {
            max_level = r.level;
        }
    }

    for (auto level = max_level;; level--) {
        for (size_t i = 0; i < runs.size();) {
            if (runs[i].level >= level) {
                auto j = i;
                while (j < runs.size() && runs[j].level >= level) {
                    j++;
                }
                if (level % 2 == 1) {
                    std::reverse(runs.begin() + static_cast<ptrdiff_t>(i),
                                 runs.begin() + static_cast<ptrdiff_t>(j));
                }
                i = j;
            } else {
                i++;
            }
        }
        if (level == 0) {
            break;
        }
    }

    return runs;
}

// ── TextShaper implementation ───────────────────────────────────────────────

void TextShaper::select_font_on_cr(cairo_t *cr, std::string_view family, float font_size, bool bold,
                                   bool italic) {
    cairo_font_options_t *fo = cairo_font_options_create();
    cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_SLIGHT);
    cairo_set_font_options(cr, fo);
    cairo_font_options_destroy(fo);

    cairo_select_font_face(cr, family.data(),
                           italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, std::round(font_size));
}

auto TextShaper::ensure_font(cairo_t *cr, float font_size, FontFamily font, bool bold, bool italic,
                             std::vector<uint32_t> const &codepoints) -> CachedFont {
    release_font();

    if (cached_size_ != font_size || cached_family_ != font || cached_bold_ != bold ||
        cached_italic_ != italic) {
        auto preferred = cairo_font_face_name(font, cr);
        cached_resolved_family_ = resolve_font_for_codepoints(preferred, codepoints);
        cached_size_ = font_size;
        cached_family_ = font;
        cached_bold_ = bold;
        cached_italic_ = italic;
    }

    select_font_on_cr(cr, cached_resolved_family_, font_size, bold, italic);

    if (!temp_surf_) {
        temp_surf_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        temp_cr_ = cairo_create(temp_surf_);
    }
    select_font_on_cr(temp_cr_, cached_resolved_family_, font_size, bold, italic);

    auto *temp_scaled = cairo_get_scaled_font(temp_cr_);
    auto *ft_face = cairo_ft_scaled_font_lock_face(temp_scaled);
    auto *hb_face = hb_ft_face_create_referenced(ft_face);
    cairo_ft_scaled_font_unlock_face(temp_scaled);
    auto *hb = hb_font_create(hb_face);
    hb_ot_font_set_funcs(hb);
    hb_face_destroy(hb_face);
    hb_font_set_scale(hb, static_cast<int>(std::round(font_size) * 64),
                      static_cast<int>(std::round(font_size) * 64));

    auto *scaled = cairo_get_scaled_font(cr);
    cairo_scaled_font_reference(scaled);

    cached_font_ = {scaled, hb};

    return cached_font_;
}

void TextShaper::release_font() {
    if (cached_font_.hb_font) {
        hb_font_destroy(cached_font_.hb_font);
        cached_font_.hb_font = nullptr;
    }
    if (cached_font_.scaled_font) {
        cairo_scaled_font_destroy(cached_font_.scaled_font);
        cached_font_.scaled_font = nullptr;
    }
}

TextShaper::~TextShaper() {
    release_font();
    if (temp_cr_) {
        cairo_destroy(temp_cr_);
    }
    if (temp_surf_) {
        cairo_surface_destroy(temp_surf_);
    }
}

auto TextShaper::shape(cairo_t *cr, std::string_view text, float font_size, FontFamily font,
                       bool bold, bool italic) -> ShapedText {
    // Short-circuit: empty text
    if (text.empty()) {
        if (cached_text_valid_) {
            cached_text_valid_ = false;
        }
        return {};
    }

    auto utf32 = utf8_to_utf32(text);
    auto len = static_cast<FriBidiStrIndex>(utf32.size());

    auto [scaled_font, hb_font] = ensure_font(cr, font_size, font, bold, italic, utf32);
    if (!hb_font || !scaled_font) {
        return {};
    }

    // Bidi analysis
    std::vector<FriBidiCharType> bidi_types(len);
    fribidi_get_bidi_types(utf32.data(), len, bidi_types.data());

    std::vector<FriBidiLevel> levels(len);
    auto par_type = static_cast<FriBidiParType>(FRIBIDI_PAR_ON);
    auto ok = fribidi_get_par_embedding_levels_ex(bidi_types.data(), nullptr, len, &par_type,
                                                  levels.data());
    if (!ok) {
        return {};
    }

    // Build logical runs
    std::vector<BidiRun> logical_runs;
    {
        int i = 0;
        while (i < len) {
            auto level = levels[i];
            auto start = i;
            while (i < len && levels[i] == level) {
                i++;
            }
            logical_runs.push_back({start, i, level});
        }
    }

    auto visual_runs = reorder_runs(std::move(logical_runs));

    // Clear any previous cached data before re-shaping
    cached_shaped_ = {};

    double pen_x = 0;
    for (auto &run : visual_runs) {
        auto run_len = run.logical_end - run.logical_start;
        if (run_len <= 0) {
            continue;
        }

        auto run_start = static_cast<size_t>(run.logical_start);
        auto run_size = static_cast<size_t>(run_len);
        std::vector<uint32_t> run_utf32(utf32.begin() + static_cast<ptrdiff_t>(run_start),
                                        utf32.begin() +
                                            static_cast<ptrdiff_t>(run_start + run_size));

        auto *buf = hb_buffer_create();
        hb_buffer_add_utf32(buf, run_utf32.data(), static_cast<int>(run_utf32.size()), 0,
                            static_cast<int>(run_utf32.size()));
        hb_buffer_set_direction(buf, run.is_rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_guess_segment_properties(buf);
        hb_shape(hb_font, buf, nullptr, 0);

        unsigned glyph_count;
        auto *info = hb_buffer_get_glyph_infos(buf, &glyph_count);
        auto *pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

        if (glyph_count == 0) {
            hb_buffer_destroy(buf);
            continue;
        }

        GlyphRun gr;
        gr.glyphs.resize(glyph_count);
        double run_advance = 0;

        if (cached_shaped_.cursor_positions.empty()) {
            cached_shaped_.cursor_positions.assign(utf32.size() + 1, -1.0);
        }

        {
            double cursor = pen_x;
            for (unsigned i = 0; i < glyph_count; i++) {
                gr.glyphs[i].index = info[i].codepoint;
                gr.glyphs[i].x = cursor + static_cast<double>(pos[i].x_offset) / 64.0;
                gr.glyphs[i].y = -static_cast<double>(pos[i].y_offset) / 64.0;
                double adv = static_cast<double>(pos[i].x_advance) / 64.0;
                cursor += adv;

                auto cp = static_cast<size_t>(run.logical_start + info[i].cluster);
                if (run.is_rtl()) {
                    cached_shaped_.cursor_positions[cp] = gr.glyphs[i].x + adv;
                    if (cp + 1 < cached_shaped_.cursor_positions.size()) {
                        cached_shaped_.cursor_positions[cp + 1] = gr.glyphs[i].x;
                    }
                } else {
                    cached_shaped_.cursor_positions[cp] = gr.glyphs[i].x;
                    if (cp + 1 < cached_shaped_.cursor_positions.size()) {
                        cached_shaped_.cursor_positions[cp + 1] = gr.glyphs[i].x + adv;
                    }
                }
            }
            run_advance = cursor - pen_x;
            pen_x = cursor;
        }

        gr.advance = run_advance;
        cached_shaped_.runs.push_back(std::move(gr));
        cached_shaped_.total_advance += run_advance;

        hb_buffer_destroy(buf);
    }

    // Fill in any unset cursor positions (ligature clusters)
    for (size_t i = 1; i <= utf32.size(); i++) {
        if (cached_shaped_.cursor_positions[i] < 0) {
            cached_shaped_.cursor_positions[i] = cached_shaped_.cursor_positions[i - 1];
        }
    }
    if (cached_shaped_.cursor_positions[0] < 0) {
        cached_shaped_.cursor_positions[0] = 0;
    }

    // Populate cache metadata
    cached_text_ = text;
    cached_text_utf32_ = std::move(utf32);
    cached_text_size_ = font_size;
    cached_text_family_ = font;
    cached_text_bold_ = bold;
    cached_text_italic_ = italic;
    cached_text_valid_ = true;

    return cached_shaped_;
}

} // namespace toolkit

#endif // TOOLKIT_HAS_TEXT_SHAPER
