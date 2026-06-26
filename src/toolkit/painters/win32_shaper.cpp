// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// clang-format off
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <usp10.h>
// clang-format on

#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/painters/win32_shaper.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace toolkit {

using text::ClusterAdvance;

namespace {

std::wstring to_wide(std::string_view s) {
    if (s.empty()) {
        return {};
    }
    auto len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    auto result = std::wstring(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

// UTF-8 run -> UTF-16, plus the UTF-8 byte offset each UTF-16 code unit came
// from (both units of a surrogate pair map to the same offset, since they
// represent a single Unicode scalar).
struct WideRun {
    std::wstring text;
    std::vector<size_t> byte_offset;
};

WideRun to_wide_run(std::string_view run_utf8) {
    WideRun out;
    out.text.reserve(run_utf8.size());
    out.byte_offset.reserve(run_utf8.size());
    size_t pos = 0;
    while (pos < run_utf8.size()) {
        auto next = Utf8Iterator::next(run_utf8, pos);
        wchar_t buf[2];
        int n = MultiByteToWideChar(CP_UTF8, 0, run_utf8.data() + pos,
                                    static_cast<int>(next - pos), buf, 2);
        for (int i = 0; i < n; ++i) {
            out.text.push_back(buf[i]);
            out.byte_offset.push_back(pos);
        }
        pos = next;
    }
    return out;
}

// One Uniscribe-itemized, shaped, placed slice of a run. Glyphs are already
// in VISUAL order -- that is ScriptShape's own guarantee, independent of the
// item's direction -- so draw_run can paint them left to right as-is.
struct ShapedItem {
    SCRIPT_ANALYSIS sa{};
    int char_start = 0; // index into the run's wide string
    int char_count = 0;
    std::vector<WORD> glyphs;
    std::vector<WORD> log_clusters; // size == char_count, indexed by logical char
    std::vector<SCRIPT_VISATTR> visattrs;
    std::vector<int> advances; // size == glyphs.size(), per-glyph pixel width
    std::vector<GOFFSET> goffsets;
};

// Itemizes `wide` and shapes+places each item. `rtl` is forced into the
// initial bidi state so Uniscribe shapes/orders consistently with the level
// bidi::BidiLine already resolved -- per docs/design/rtl-line-input.md
// section 6, Uniscribe is only used for shaping here, never for re-deriving
// direction.
std::vector<ShapedItem> shape_items(HDC hdc, SCRIPT_CACHE *cache, std::wstring const &wide,
                                    bool rtl) {
    std::vector<ShapedItem> result;
    if (wide.empty()) {
        return result;
    }

    SCRIPT_CONTROL control{};
    SCRIPT_STATE state{};
    state.uBidiLevel = rtl ? 1 : 0;
    state.fOverrideDirection = 1;

    auto max_items = static_cast<int>(wide.size()) + 2;
    auto items = std::vector<SCRIPT_ITEM>(static_cast<size_t>(max_items));
    auto num_items = 0;
    auto hr = ScriptItemize(wide.c_str(), static_cast<int>(wide.size()), max_items - 1,
                               &control, &state, items.data(), &num_items);
    if (FAILED(hr)) {
        return result;
    }

    result.reserve(static_cast<size_t>(num_items));
    for (auto i = 0; i < num_items; ++i) {
        ShapedItem item;
        item.sa = items[i].a;
        item.char_start = items[i].iCharPos;
        item.char_count = items[i + 1].iCharPos - items[i].iCharPos;
        if (item.char_count <= 0) {
            continue;
        }

        auto chars = wide.c_str() + item.char_start;
        auto max_glyphs = item.char_count * 2 + 16;
        item.log_clusters.resize(static_cast<size_t>(item.char_count));

        auto num_glyphs = 0;
        HRESULT shr = E_OUTOFMEMORY;
        for (auto attempt = 0; attempt < 3 && shr == E_OUTOFMEMORY; ++attempt) {
            item.glyphs.resize(static_cast<size_t>(max_glyphs));
            item.visattrs.resize(static_cast<size_t>(max_glyphs));
            shr = ScriptShape(hdc, cache, chars, item.char_count, max_glyphs, &item.sa,
                              item.glyphs.data(), item.log_clusters.data(), item.visattrs.data(),
                              &num_glyphs);
            max_glyphs *= 2;
        }
        if (FAILED(shr)) {
            // Font doesn't support this script's shaping tables -- fall back to a
            // script-agnostic shape (no joining/ligatures) rather than dropping the item.
            item.sa.eScript = SCRIPT_UNDEFINED;
            shr = ScriptShape(hdc, cache, chars, item.char_count, max_glyphs, &item.sa,
                              item.glyphs.data(), item.log_clusters.data(), item.visattrs.data(),
                              &num_glyphs);
        }
        if (FAILED(shr)) {
            continue;
        }
        item.glyphs.resize(static_cast<size_t>(num_glyphs));
        item.visattrs.resize(static_cast<size_t>(num_glyphs));

        item.advances.resize(static_cast<size_t>(num_glyphs));
        item.goffsets.resize(static_cast<size_t>(num_glyphs));
        ABC abc{};
        HRESULT phr = ScriptPlace(hdc, cache, item.glyphs.data(), num_glyphs, item.visattrs.data(),
                                  &item.sa, item.advances.data(), item.goffsets.data(), &abc);
        if (FAILED(phr)) {
            continue;
        }

        result.push_back(std::move(item));
    }
    return result;
}

// Pixel width of logical character `ci` (0-based, item-relative) within its
// own visual slot. ScriptCPtoX already divides a shared glyph's width evenly
// across the characters that produced it (ligatures) and is direction-aware,
// so this needs no manual cluster bookkeeping.
int char_width(ShapedItem const &item, int ci) {
    auto x0 = 0, x1 = 0;
    ScriptCPtoX(ci, FALSE, item.char_count, static_cast<int>(item.glyphs.size()),
               item.log_clusters.data(), item.visattrs.data(), item.advances.data(), &item.sa,
               &x0);
    ScriptCPtoX(ci, TRUE, item.char_count, static_cast<int>(item.glyphs.size()),
               item.log_clusters.data(), item.visattrs.data(), item.advances.data(), &item.sa,
               &x1);
    return x1 > x0 ? x1 - x0 : x0 - x1;
}

// One ClusterAdvance per UTF-8 scalar (v1 contract -- see ClusterAdvance's
// docs), in the item's LOGICAL order. A surrogate pair is two UTF-16 code
// units mapping to the same scalar's byte offset; their per-unit widths are
// summed back into a single entry.
std::vector<ClusterAdvance> item_to_scalars(ShapedItem const &item,
                                            std::vector<size_t> const &byte_offset) {
    std::vector<ClusterAdvance> out;
    for (auto ci = 0; ci < item.char_count;) {
        auto off = byte_offset[static_cast<size_t>(item.char_start + ci)];
        auto next_ci = ci + 1;
        while (next_ci < item.char_count &&
              byte_offset[static_cast<size_t>(item.char_start + next_ci)] == off) {
            ++next_ci;
        }
        auto width = 0.0f;
        for (auto j = ci; j < next_ci; ++j) {
            width += static_cast<float>(char_width(item, j));
        }
        out.push_back({off, width});
        ci = next_ci;
    }
    return out;
}

} // namespace

struct Win32Shaper::Impl {
    // private, off-screen DC used only to measure (shape_run)
    HDC hdc = nullptr;

    HFONT create_font(float font_size, FontFamily family) {
        auto const &t = Theme::current();
        auto face_name =
            (family == FontFamily::Monospace) ? t.palette.fonts.monospace : t.palette.fonts.system;
        auto wface = to_wide(face_name);
        auto height = -static_cast<int>(std::round(font_size));
        auto pitch =
            (family == FontFamily::Monospace) ? FIXED_PITCH | FF_MODERN : DEFAULT_PITCH | FF_SWISS;
        return CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, pitch,
                           wface.c_str());
    }
};

Win32Shaper::Win32Shaper() : impl_(std::make_unique<Impl>()) {
    impl_->hdc = CreateCompatibleDC(nullptr);
}

Win32Shaper::~Win32Shaper() {
    if (impl_->hdc) {
        DeleteDC(impl_->hdc);
    }
}

std::vector<ClusterAdvance> Win32Shaper::shape_run(std::string_view run_utf8, bool rtl,
                                                   float font_size, FontFamily font) {
    std::vector<ClusterAdvance> result;
    if (run_utf8.empty()) {
        return result;
    }

    auto wide = to_wide_run(run_utf8);
    if (wide.text.empty()) {
        return result;
    }

    auto hfont = impl_->create_font(font_size, font);
    auto old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    // SCRIPT_CACHE is tied to one specific font; shape_run/draw_run may be
    // called with varying font_size/family, so each call gets its own
    // short-lived cache rather than risking a stale one in Impl.
    SCRIPT_CACHE cache = nullptr;
    auto items = shape_items(impl_->hdc, &cache, wide.text, rtl);
    if (cache) {
        ScriptFreeCache(&cache);
    }

    SelectObject(impl_->hdc, old_font);
    DeleteObject(hfont);

    if (items.empty()) {
        return result;
    }

    std::vector<std::vector<ClusterAdvance>> per_item(items.size());
    for (auto i = 0; i < items.size(); ++i) {
        per_item[i] = item_to_scalars(items[i], wide.byte_offset);
        if (rtl) {
            std::reverse(per_item[i].begin(), per_item[i].end());
        }
    }

    if (!rtl) {
        for (auto &entries : per_item) {
            result.insert(result.end(), entries.begin(), entries.end());
        }
    } else {
        for (auto it = per_item.rbegin(); it != per_item.rend(); ++it) {
            result.insert(result.end(), it->begin(), it->end());
        }
    }
    return result;
}

void Win32Shaper::draw_run(Painter &painter, std::string_view run_utf8, bool rtl, Point origin,
                           Color const &color, float font_size, FontFamily font) {
    if (run_utf8.empty()) {
        return;
    }

    auto *gdi_painter = dynamic_cast<GDIPainter *>(&painter);
    if (!gdi_painter) {
        return; // v1: only the GDI+ backend can interop with Uniscribe's HDC-based API
    }

    auto wide = to_wide_run(run_utf8);
    if (wide.text.empty()) {
        return;
    }

    auto *graphics = static_cast<Gdiplus::Graphics *>(gdi_painter->graphics());
    auto scale = gdi_painter->scale();

    // GetHDC() hands back the raw device HDC -- it does not carry over the
    // Graphics object's current world transform (the TranslateTransform/
    // ScaleTransform stack built up while painting down the widget tree), so
    // `origin` has to be mapped from widget-local space to device pixels by
    // hand, same as Win32TextRasterizer::draw_text does via DrawString.
    Gdiplus::Matrix matrix;
    graphics->GetTransform(&matrix);
    Gdiplus::PointF device_origin(origin.x, origin.y);
    matrix.TransformPoints(&device_origin, 1);

    auto hdc = graphics->GetHDC();
    if (!hdc) {
        return;
    }

    HFONT hfont = impl_->create_font(font_size * scale, font);
    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, hfont));

    auto clamp = [](float v) {
        if (v < 0.0f) {
            return 0;
        }
        if (v > 1.0f) {
            return 255;
        }
        return static_cast<int>(std::round(v * 255));
    };
    SetTextColor(hdc, RGB(clamp(color.r), clamp(color.g), clamp(color.b)));
    SetBkMode(hdc, TRANSPARENT);
    // A fresh HDC defaults to TA_TOP, but `origin` is the baseline (same
    // convention as Win32TextRasterizer::draw_text's DrawString call).
    SetTextAlign(hdc, TA_LEFT | TA_BASELINE);

    SCRIPT_CACHE cache = nullptr;
    auto items = shape_items(hdc, &cache, wide.text, rtl);

    auto draw_item = [&](ShapedItem const &item) {
        ScriptTextOut(hdc, &cache, static_cast<int>(std::round(device_origin.X)),
                     static_cast<int>(std::round(device_origin.Y)), 0, nullptr, &item.sa, nullptr, 0,
                     item.glyphs.data(), static_cast<int>(item.glyphs.size()),
                     item.advances.data(), nullptr, item.goffsets.data());
        auto width = 0;
        for (auto a : item.advances) {
            width += a;
        }
        device_origin.X += static_cast<float>(width);
    };

    if (!rtl) {
        for (auto const &item : items) {
            draw_item(item);
        }
    } else {
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            draw_item(*it);
        }
    }

    if (cache) {
        ScriptFreeCache(&cache);
    }
    SelectObject(hdc, old_font);
    DeleteObject(hfont);
    graphics->ReleaseHDC(hdc);
}

} // namespace toolkit

#endif // _WIN32
