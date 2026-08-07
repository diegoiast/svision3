// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

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
// clang-format on

#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace toolkit {

static std::wstring to_wide(std::string_view s) {
    if (s.empty()) {
        return {};
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

static Gdiplus::Color to_gdiplus_color(Color const &c) {
    auto clamp = [](float v) {
        if (v < 0) {
            return 0;
        }
        if (v > 1) {
            return 255;
        }
        return static_cast<int>(std::round(v * 255));
    };
    return Gdiplus::Color(static_cast<BYTE>(clamp(c.a)), static_cast<BYTE>(clamp(c.r)),
                          static_cast<BYTE>(clamp(c.g)), static_cast<BYTE>(clamp(c.b)));
}

struct Win32TextRasterizer::Impl {
    HDC hdc = nullptr;

    struct MetricsCache {
        float font_size = -1.0f;
        FontFamily family = FontFamily::System;
        Painter::FontMetrics metrics{};
    };
    std::optional<MetricsCache> cached_metrics;
    std::unordered_map<std::string, Size> measure_cache;

    struct HFontKey {
        std::string face;
        int height = 0; // negative pixel height (font_size * scale)
        bool bold = false;
        bool italic = false;
        bool operator==(HFontKey const &o) const {
            return height == o.height && bold == o.bold && italic == o.italic && face == o.face;
        }
    };
    struct HFontKeyHash {
        size_t operator()(HFontKey const &k) const {
            size_t h = std::hash<std::string>{}(k.face);
            h ^= std::hash<int>{}(k.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.bold)   + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.italic) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<HFontKey, HFONT, HFontKeyHash> hfont_cache;

    struct GdiFontKey {
        std::string face;
        float size = 0;
        int style = 0; // Gdiplus::FontStyle int
        bool operator==(GdiFontKey const &o) const {
            return size == o.size && style == o.style && face == o.face;
        }
    };
    struct GdiFontKeyHash {
        size_t operator()(GdiFontKey const &k) const {
            size_t h = std::hash<std::string>{}(k.face);
            h ^= std::hash<float>{}(k.size)  + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.style)   + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    // Gdiplus::FontFamily and Font are not copyable; store as unique_ptr pairs
    struct GdiFontEntry {
        std::unique_ptr<Gdiplus::FontFamily> family;
        std::unique_ptr<Gdiplus::Font>       font;
    };
    std::unordered_map<GdiFontKey, GdiFontEntry, GdiFontKeyHash> gdi_font_cache;

    // Rotated text goes the long way round -- rasterize() allocates a DIB section, runs TextOutW,
    // flushes GDI and walks every pixel to build a coverage bitmap. That is ~5x the cost of a
    // plain DrawString, and the text involved (e.g. vertical dock tab labels) is static, so
    // without this it is re-rasterised on every frame. Keyed on everything rasterize() consumes.
    struct RasterKey {
        std::string text;
        float font_size = 0;
        float scale = 0;
        int family = 0;
        bool bold = false, italic = false;
        uint32_t argb = 0; // rasterize() bakes colour.a into the coverage
        bool operator==(RasterKey const &o) const {
            return font_size == o.font_size && scale == o.scale && family == o.family &&
                   bold == o.bold && italic == o.italic && argb == o.argb && text == o.text;
        }
    };
    struct RasterKeyHash {
        size_t operator()(RasterKey const &k) const {
            size_t h = std::hash<std::string>{}(k.text);
            auto mix = [&h](size_t v) { h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2); };
            mix(std::hash<float>{}(k.font_size));
            mix(std::hash<float>{}(k.scale));
            mix(std::hash<int>{}(k.family));
            mix(std::hash<uint32_t>{}(k.argb));
            mix(static_cast<size_t>(k.bold) | (static_cast<size_t>(k.italic) << 1));
            return h;
        }
    };
    std::unordered_map<RasterKey, RasterizedText, RasterKeyHash> raster_cache;

    ~Impl() {
        for (auto &[k, hf] : hfont_cache) {
            if (hf) DeleteObject(hf);
        }
        // gdi_font_cache entries are unique_ptr â€” cleaned up automatically
        if (hdc) DeleteDC(hdc);
    }

    HFONT get_hfont(float font_size, float scale, FontFamily family, bool bold, bool italic) {
        auto const &t = Theme::current();
        std::string face =
            (family == FontFamily::Monospace) ? t.palette.fonts.monospace : t.palette.fonts.system;
        int height = -static_cast<int>(std::round(font_size * scale));
        HFontKey key{face, height, bold, italic};
        auto it = hfont_cache.find(key);
        if (it != hfont_cache.end()) {
            return it->second;
        }
        auto wface = to_wide(face);
        DWORD pitch =
            (family == FontFamily::Monospace) ? FIXED_PITCH | FF_MODERN : DEFAULT_PITCH | FF_SWISS;
        HFONT hf = CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, italic ? TRUE : FALSE,
                               FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, pitch, wface.c_str());
        hfont_cache.emplace(key, hf);
        return hf;
    }

    Gdiplus::Font *get_gdi_font(std::string const &face, float font_size, int gdi_style) {
        GdiFontKey key{face, font_size, gdi_style};
        auto it = gdi_font_cache.find(key);
        if (it != gdi_font_cache.end()) {
            return it->second.font.get();
        }
        auto wface = to_wide(face);
        auto ff = std::make_unique<Gdiplus::FontFamily>(wface.c_str());
        auto font = std::make_unique<Gdiplus::Font>(ff.get(), font_size,
                                                    gdi_style, Gdiplus::UnitPixel);
        auto *ptr = font.get();
        gdi_font_cache.emplace(key, GdiFontEntry{std::move(ff), std::move(font)});
        return ptr;
    }
};

Win32TextRasterizer::Win32TextRasterizer() : impl_(std::make_unique<Impl>()) {
    impl_->hdc = CreateCompatibleDC(nullptr);
    SetBkMode(impl_->hdc, TRANSPARENT);
}

Win32TextRasterizer::~Win32TextRasterizer() = default;

RasterizedText Win32TextRasterizer::rasterize(std::string_view text, float font_size, float scale,
                                              Color const &color, FontFamily font, bool bold,
                                              bool italic) {
    auto wtext = to_wide(text);
    if (wtext.empty()) {
        return {};
    }

    HFONT hfont = impl_->get_hfont(font_size, scale, font, bold, italic);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    SIZE sz;
    GetTextExtentPoint32W(impl_->hdc, wtext.c_str(), static_cast<int>(wtext.size()), &sz);

    int w = sz.cx;
    int h = sz.cy;
    if (w <= 0 || h <= 0) {
        SelectObject(impl_->hdc, old_font);
        return {};
    }

    TEXTMETRICW tm;
    GetTextMetricsW(impl_->hdc, &tm);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP hbm = CreateDIBSection(impl_->hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm || !bits) {
        SelectObject(impl_->hdc, old_font);
        return {};
    }

    HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(impl_->hdc, hbm));

    RECT rc = {0, 0, w, h};
    FillRect(impl_->hdc, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    SetTextColor(impl_->hdc, RGB(255, 255, 255));
    SetBkMode(impl_->hdc, TRANSPARENT);
    TextOutW(impl_->hdc, 0, 0, wtext.c_str(), static_cast<int>(wtext.size()));
    GdiFlush();

    auto *src = static_cast<uint8_t *>(bits);
    RasterizedText result;
    result.pixels.resize(w * h * 4);
    result.width = w;
    result.height = h;
    result.ascent = static_cast<float>(tm.tmAscent) / scale;

    float tint_a = std::clamp(color.a, 0.0f, 1.0f);
    for (int i = 0; i < w * h; i++) {
        // GDI TextOut on a 32bpp DIB doesn't touch the alpha channel (it remains 0 from the
        // BlackBrush fill). Since we drew white on black, the coverage is in the RGB channels.
        // We use the green channel as a good approximation for the coverage.
        uint8_t coverage = src[i * 4 + 1];
        float alpha = (coverage / 255.0f) * tint_a;

        result.pixels[i * 4 + 0] = 255;
        result.pixels[i * 4 + 1] = 255;
        result.pixels[i * 4 + 2] = 255;
        result.pixels[i * 4 + 3] = static_cast<uint8_t>(alpha * 255.0f);
    }

    SelectObject(impl_->hdc, old_bm);
    DeleteObject(hbm);
    SelectObject(impl_->hdc, old_font);

    return result;
}

Size Win32TextRasterizer::measure(std::string_view text, float font_size, FontFamily font,
                                  bool bold, bool italic) {
    if (text.empty()) {
        return {0, 0};
    }

    auto cache_key = std::string(text) + '\0' + std::to_string(font_size) + '\0' +
                     (font == FontFamily::Monospace ? '1' : '0') + (bold ? 'b' : '_') +
                     (italic ? 'i' : '_');
    auto it = impl_->measure_cache.find(cache_key);
    if (it != impl_->measure_cache.end()) {
        return it->second;
    }

    auto wtext = to_wide(text);
    HFONT hfont = impl_->get_hfont(font_size, 1.0f, font, bold, italic);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    SIZE sz;
    GetTextExtentPoint32W(impl_->hdc, wtext.c_str(), static_cast<int>(wtext.size()), &sz);

    SelectObject(impl_->hdc, old_font);

    auto result = Size{static_cast<float>(sz.cx), static_cast<float>(sz.cy)};
    impl_->measure_cache.emplace(std::move(cache_key), result);
    return result;
}

Painter::FontMetrics Win32TextRasterizer::metrics(float font_size, FontFamily family) {
    if (impl_->cached_metrics.has_value() && impl_->cached_metrics->font_size == font_size &&
        impl_->cached_metrics->family == family) {
        return impl_->cached_metrics->metrics;
    }

    auto const &t = Theme::current();
    std::string face_name =
        (family == FontFamily::Monospace) ? t.palette.fonts.monospace : t.palette.fonts.system;
    auto wface = to_wide(face_name);

    Gdiplus::FontFamily ff(wface.c_str());
    Gdiplus::FontStyle style = Gdiplus::FontStyleRegular;

    float em_height = static_cast<float>(ff.GetEmHeight(style));
    float ascent = font_size * static_cast<float>(ff.GetCellAscent(style)) / em_height;
    float descent = font_size * static_cast<float>(ff.GetCellDescent(style)) / em_height;
    float line_spacing = font_size * static_cast<float>(ff.GetLineSpacing(style)) / em_height;

    auto result = Painter::FontMetrics{ascent, descent, line_spacing};
    impl_->cached_metrics = {font_size, family, result};
    return result;
}

struct GDIPainter::Impl {
    Gdiplus::Graphics *graphics;
    bool owned;
    std::vector<Gdiplus::GraphicsState> state_stack;
    float scale;
    Painter::LineStyle line_style = Painter::LineStyle::Solid;

    Impl(HDC hdc, float s) : owned(true), scale(s) {
        graphics = new Gdiplus::Graphics(hdc);
        graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        graphics->ScaleTransform(s, s);
    }

    Impl(Gdiplus::Graphics *g, float s) : graphics(g), owned(false), scale(s) {
        graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        graphics->ScaleTransform(s, s);
    }

    ~Impl() {
        if (owned) {
            delete graphics;
        }
    }
};

GDIPainter::GDIPainter(void *hdc, float scale, TextRasterizer *rasterizer)
    : Painter(rasterizer), impl_(std::make_unique<Impl>(static_cast<HDC>(hdc), scale)) {}

GDIPainter::GDIPainter(Gdiplus::Graphics *g, float scale, TextRasterizer *rasterizer)
    : Painter(rasterizer), impl_(std::make_unique<Impl>(g, scale)) {}

GDIPainter::~GDIPainter() {}

float GDIPainter::scale() const { return impl_->scale; }
void *GDIPainter::graphics() { return impl_->graphics; }

void GDIPainter::push_clip(Rect const &r) {
    float s = impl_->scale;
    float x = std::floor(r.x * s) / s;
    float y = std::floor(r.y * s) / s;
    float w = std::ceil((r.x + r.width) * s) / s - x;
    float h = std::ceil((r.y + r.height) * s) / s - y;

    impl_->state_stack.push_back(impl_->graphics->Save());
    impl_->graphics->SetClip(Gdiplus::RectF(x, y, w, h), Gdiplus::CombineModeIntersect);
}

void GDIPainter::push_clip(Rect const &r, float radius) {
    if (radius <= 0) {
        push_clip(r);
        return;
    }
    float s = impl_->scale;
    float x = std::floor(r.x * s) / s;
    float y = std::floor(r.y * s) / s;
    float w = std::ceil((r.x + r.width) * s) / s - x;
    float h = std::ceil((r.y + r.height) * s) / s - y;
    float d = radius * 2;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    impl_->state_stack.push_back(impl_->graphics->Save());
    impl_->graphics->SetClip(&path, Gdiplus::CombineModeIntersect);
}

void GDIPainter::pop_clip() {
    if (!impl_->state_stack.empty()) {
        impl_->graphics->Restore(impl_->state_stack.back());
        impl_->state_stack.pop_back();
    }
}

void GDIPainter::push_translation(Point p) {
    float s = impl_->scale;
    float x = std::floor(p.x * s + 0.5f) / s;
    float y = std::floor(p.y * s + 0.5f) / s;

    impl_->state_stack.push_back(impl_->graphics->Save());
    impl_->graphics->TranslateTransform(x, y);
}

void GDIPainter::pop_translation() {
    if (!impl_->state_stack.empty()) {
        impl_->graphics->Restore(impl_->state_stack.back());
        impl_->state_stack.pop_back();
    }
}

void GDIPainter::push_rotation(float degrees) {
    impl_->state_stack.push_back(impl_->graphics->Save());
    impl_->graphics->RotateTransform(degrees);
}

void GDIPainter::pop_rotation() {
    if (!impl_->state_stack.empty()) {
        impl_->graphics->Restore(impl_->state_stack.back());
        impl_->state_stack.pop_back();
    }
}

void GDIPainter::set_line_style(Painter::LineStyle style) { impl_->line_style = style; }

static void apply_line_style(Gdiplus::Pen &pen, Painter::LineStyle style, float lw) {
    switch (style) {
    case Painter::LineStyle::Dashed: {
        pen.SetDashStyle(Gdiplus::DashStyleDash);
        break;
    }
    case Painter::LineStyle::Dotted: {
        pen.SetDashStyle(Gdiplus::DashStyleDot);
        break;
    }
    case Painter::LineStyle::Solid:
    default:
        pen.SetDashStyle(Gdiplus::DashStyleSolid);
        break;
    }
}

void GDIPainter::fill_rect(Rect const &r, Color const &c) {
    float s = impl_->scale;
    float x = std::floor(r.x * s) / s;
    float y = std::floor(r.y * s) / s;
    float w = std::ceil((r.x + r.width) * s) / s - x;
    float h = std::ceil((r.y + r.height) * s) / s - y;

    Gdiplus::SmoothingMode old = impl_->graphics->GetSmoothingMode();
    impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

    Gdiplus::SolidBrush brush(to_gdiplus_color(c));
    impl_->graphics->FillRectangle(&brush, x, y, w, h);

    impl_->graphics->SetSmoothingMode(old);
}

void GDIPainter::draw_rect(Rect const &r, Color const &c, float lw) {
    auto s = impl_->scale;
    auto fx = std::floor(r.x * s);
    auto fy = std::floor(r.y * s);
    auto lx = std::ceil((r.x + r.width) * s);
    auto ly = std::ceil((r.y + r.height) * s);
    auto slw = std::max(1.0f, std::round(lw * s));

    Gdiplus::SmoothingMode old = impl_->graphics->GetSmoothingMode();
    impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

    Gdiplus::Pen pen(to_gdiplus_color(c), slw / s);
    apply_line_style(pen, impl_->line_style, slw / s);

    // To color pixels from fx to lx-1 (inclusive), width must be (lx-1) - fx.
    float dw = (lx - fx - 1.0f) / s;
    float dh = (ly - fy - 1.0f) / s;
    if (dw < 0) {
        dw = 0;
    }
    if (dh < 0) {
        dh = 0;
    }

    impl_->graphics->DrawRectangle(&pen, fx / s, fy / s, dw, dh);

    impl_->graphics->SetSmoothingMode(old);
}

// Anti-aliasing the whole shape makes GDI+ walk every pixel of it, which costs tens of
// milliseconds for something as large as a window background. Only the rows containing the
// corners actually curve; everything between them is a plain rectangle.
//
// So for large fills, draw it in three phases: the top band (both top corners), a plain
// non-anti-aliased rectangle for the middle, and the bottom band (both bottom corners). The
// bands are drawn by clipping the *whole* path to the band rather than by filling a partial
// shape â€” inside a band the path has no horizontal edge at the cut, so the pixels along the
// cut are fully covered and there is no seam against the middle rectangle. (Splitting the
// shape itself, e.g. into rectangles plus corner wedges, does seam: the wedges' straight
// edges get anti-aliased and end up half-blended with the background.)
//
// The split is only taken for large fills, gated on the *area* of the middle band (in device
// pixels, since that is what GDI+ actually rasterises). A purely geometric rule such as
// "h > 3*rad" describes when the split is applicable, but not when it is worth taking, and
// measuring both showed the area gate is the one that matters: on demo_dock the geometric
// rule split 18-20 shapes instead of 7 for no measurable speed-up (the extra shapes are
// small, so the anti-aliasing it avoids is negligible), while producing 200+ differing
// pixels against the single-fill reference. The area gate renders byte-identically.
//
// The extra shapes differing is not currently explained; until it is, keep the gate
// conservative so only large fills â€” where the win was measured at 13-35ms -> 1-4ms â€” take
// this path, and everything else keeps the plain single-path fill.
//
// The band height is rounded *up* to a whole device pixel: the cut then lands on a pixel
// boundary (so the aliased clip edge is crisp) and at or below the arcs' tangent point (so
// the cut crosses the shape where its sides are already straight and full width). The path
// itself still uses the caller's radius, so the corners rasterise exactly as they would
// have with a single fill.
//
// Minimum middle-band area, in device pixels, before fill_rounded_rect splits the fill into
// banded phases (see below). ~500x500; chosen to sit well clear of ordinary widget-sized fills
// on both sides rather than tuned to a measured crossover.
static constexpr float kSplitMinArea = 250000.0f;

void GDIPainter::fill_rounded_rect(Rect const &r, Color const &c, float rad) {
    if (rad <= 0) {
        fill_rect(r, c);
        return;
    }
    auto s = impl_->scale;
    auto x = std::floor(r.x * s) / s;
    auto y = std::floor(r.y * s) / s;
    auto w = std::ceil((r.x + r.width) * s) / s - x;
    auto h = std::ceil((r.y + r.height) * s) / s - y;
    auto d = rad * 2;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    Gdiplus::SolidBrush brush(to_gdiplus_color(c));

    auto band = std::ceil(rad * s) / s;
    band = std::min(band, h / 2.0f);
    auto mid_h = h - 2 * band;
    if (band <= 0 || mid_h <= 0 || (w * s) * (mid_h * s) <= kSplitMinArea) {
        impl_->graphics->FillPath(&brush, &path);
        return;
    }

    auto fill_band = [&](float by, float bh) {
        // Save/Restore and CombineModeIntersect so an outer clip (e.g. the window's rounded
        // corner clip pushed by push_clip) is preserved rather than replaced.
        Gdiplus::GraphicsState st = impl_->graphics->Save();
        impl_->graphics->SetClip(Gdiplus::RectF(x, by, w, bh), Gdiplus::CombineModeIntersect);
        impl_->graphics->FillPath(&brush, &path);
        impl_->graphics->Restore(st);
    };
    fill_band(y, band);
    fill_band(y + h - band, band);

    Gdiplus::SmoothingMode old = impl_->graphics->GetSmoothingMode();
    impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    impl_->graphics->FillRectangle(&brush, x, y + band, w, mid_h);
    impl_->graphics->SetSmoothingMode(old);
}

void GDIPainter::draw_rounded_rect(Rect const &r, Color const &c, float rad, float lw) {
    if (rad <= 0) {
        draw_rect(r, c, lw);
        return;
    }
    auto s = impl_->scale;
    auto slw = std::max(1.0f, std::round(lw * s)) / s;
    auto fx = std::floor(r.x * s);
    auto fy = std::floor(r.y * s);
    auto lx = std::ceil((r.x + r.width) * s);
    auto ly = std::ceil((r.y + r.height) * s);
    // For anti-aliased rounded rects, we center the stroke on the outer pixel edge
    auto x = (fx + 0.5f) / s;
    auto y = (fy + 0.5f) / s;
    auto w = (lx - fx - 1.0f) / s;
    auto h = (ly - fy - 1.0f) / s;
    auto d = rad * 2;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    Gdiplus::Pen pen(to_gdiplus_color(c), slw);
    apply_line_style(pen, impl_->line_style, slw);
    impl_->graphics->DrawPath(&pen, &path);
}

void GDIPainter::fill_triangle(Point a, Point b, Point c, Color const &color) {
    auto s = impl_->scale;
    auto snap = [s](Point p) {
        return Gdiplus::PointF(std::floor(p.x * s) / s, std::floor(p.y * s) / s);
    };
    Gdiplus::PointF pts[3] = {snap(a), snap(b), snap(c)};
    Gdiplus::SolidBrush brush(to_gdiplus_color(color));
    impl_->graphics->FillPolygon(&brush, pts, 3);
}

void GDIPainter::fill_polygon(std::vector<Point> const &points, Color const &color) {
    if (points.size() < 3) {
        return;
    }
    auto s = impl_->scale;
    auto snap = [s](Point p) {
        return Gdiplus::PointF(std::floor(p.x * s) / s, std::floor(p.y * s) / s);
    };
    std::vector<Gdiplus::PointF> pts;
    pts.reserve(points.size());
    for (auto const &p : points) {
        pts.push_back(snap(p));
    }
    Gdiplus::SolidBrush brush(to_gdiplus_color(color));
    impl_->graphics->FillPolygon(&brush, pts.data(), static_cast<int>(pts.size()));
}

void GDIPainter::draw_line(Point a, Point b, Color const &c, float lw) {
    auto s = impl_->scale;
    auto slw = std::max(1.0f, std::round(lw * s)) / s;
    auto horizontal = std::abs(a.y - b.y) < 0.001f;
    auto vertical = std::abs(a.x - b.x) < 0.001f;
    auto axis_aligned = horizontal || vertical;

    if (axis_aligned && impl_->line_style == Painter::LineStyle::Solid) {
        // Fill an exact pixel rect instead of stroking. DrawLine's pen-centered
        // stroke covers the endpoint pixels only partially, and GDI+ drops the
        // last pixel unpredictably â€” leaving gaps where frame corners meet.
        // Both endpoints are inclusive here, matching classic Win32 bevel math.
        auto thickness = std::max(1.0f, std::round(lw * s));
        Gdiplus::SolidBrush brush(to_gdiplus_color(c));
        if (horizontal) {
            auto xs = std::floor(std::min(a.x, b.x) * s);
            auto xe = std::floor(std::max(a.x, b.x) * s) + 1.0f;
            auto ys = std::floor(a.y * s) - std::floor((thickness - 1.0f) / 2.0f);
            impl_->graphics->FillRectangle(&brush, xs / s, ys / s, (xe - xs) / s,
                                           thickness / s);
        } else {
            auto ys = std::floor(std::min(a.y, b.y) * s);
            auto ye = std::floor(std::max(a.y, b.y) * s) + 1.0f;
            auto xs = std::floor(a.x * s) - std::floor((thickness - 1.0f) / 2.0f);
            impl_->graphics->FillRectangle(&brush, xs / s, ys / s, thickness / s,
                                           (ye - ys) / s);
        }
        return;
    }

    Gdiplus::SmoothingMode old = impl_->graphics->GetSmoothingMode();
    if (axis_aligned) {
        impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    }

    auto x1 = (std::floor(a.x * s) + 0.5f) / s;
    auto y1 = (std::floor(a.y * s) + 0.5f) / s;
    auto x2 = (std::floor(b.x * s) + 0.5f) / s;
    auto y2 = (std::floor(b.y * s) + 0.5f) / s;
    Gdiplus::Pen pen(to_gdiplus_color(c), slw);

    apply_line_style(pen, impl_->line_style, slw);
    impl_->graphics->DrawLine(&pen, x1, y1, x2, y2);
    if (axis_aligned) {
        impl_->graphics->SetSmoothingMode(old);
    }
}

void GDIPainter::draw_polyline(std::vector<Point> const &points, Color const &color, float lw) {
    if (points.size() < 2) {
        return;
    }
    auto s = impl_->scale;
    auto slw = std::max(1.0f, std::round(lw * s)) / s;
    std::vector<Gdiplus::PointF> pts;
    pts.reserve(points.size());
    for (auto const &p : points) {
        pts.push_back(Gdiplus::PointF((std::floor(p.x * s) + 0.5f) / s,
                                      (std::floor(p.y * s) + 0.5f) / s));
    }
    Gdiplus::Pen pen(to_gdiplus_color(color), slw);
    apply_line_style(pen, impl_->line_style, slw);
    impl_->graphics->DrawLines(&pen, pts.data(), static_cast<int>(pts.size()));
}

void GDIPainter::fill_circle(Point center, float radius, Color const &c) {
    Gdiplus::SolidBrush brush(to_gdiplus_color(c));
    impl_->graphics->FillEllipse(&brush, center.x - radius, center.y - radius,
                                  radius * 2, radius * 2);
}

void GDIPainter::draw_circle(Point center, float radius, Color const &c, float lw) {
    float s = impl_->scale;
    float slw = std::max(1.0f, std::round(lw * s)) / s;
    Gdiplus::Pen pen(to_gdiplus_color(c), slw);
    apply_line_style(pen, impl_->line_style, slw);
    impl_->graphics->DrawEllipse(&pen, center.x - radius, center.y - radius,
                                  radius * 2, radius * 2);
}

void Win32TextRasterizer::draw_text(Painter &p, std::string_view text, Point pos, Color const &c,
                                    float font_size, FontFamily family,
                                    Painter::TextOrientation orientation, bool bold, bool italic) {
    if (auto *gp = dynamic_cast<GDIPainter *>(&p)) {
        if (text.empty()) {
            return;
        }
        auto wtext = to_wide(text);
        if (wtext.empty()) {
            return;
        }

        auto const &t = Theme::current();
        auto const &face =
            (family == FontFamily::Monospace) ? t.palette.fonts.monospace : t.palette.fonts.system;

        auto *graphics = static_cast<Gdiplus::Graphics *>(gp->graphics());
        auto scale = gp->scale();

        int gdi_style = Gdiplus::FontStyleRegular;
        if (bold && italic) {
            gdi_style = Gdiplus::FontStyleBoldItalic;
        } else if (bold) {
            gdi_style = Gdiplus::FontStyleBold;
        } else if (italic) {
            gdi_style = Gdiplus::FontStyleItalic;
        }
        auto *font = impl_->get_gdi_font(face, font_size, gdi_style);
        Gdiplus::SolidBrush brush(to_gdiplus_color(c));

        // GenericTypographic removes GDI+'s default internal margins.
        Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
        format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsNoFitBlackBox |
                              Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

        // Use the rasterizer's metrics ascent so that draw_text aligns with the baseline_y
        // that layout code computes via painter.font_metrics() â€” both come from the same GDI path.
        auto ascent = metrics(font_size, family).ascent;

        // DrawString places the origin at the top-left of the layout box; pos.y is the baseline.
        if (orientation == Painter::TextOrientation::Horizontal) {
            Gdiplus::PointF draw_at(pos.x, pos.y - ascent);
            graphics->DrawString(wtext.c_str(), -1, font, draw_at, &format, &brush);
        } else {
            // Rotating the GDI+ context before DrawString disables ClearType (subpixel layout
            // is undefined after rotation), producing pixelated greyscale-AA text.
            // Instead, rasterize the text horizontally (full ClearType quality), colorize the
            // resulting bitmap, then draw it rotated.  At exactly Â±90Â° the pixel mapping is
            // 1-to-1 so there are no interpolation artifacts from the rotation itself.
            // Cached: see raster_cache. rasterize() is expensive and this text is typically
            // static, so re-running it every frame is pure waste.
            auto to_u8 = [](float v) {
                return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            Impl::RasterKey rkey{std::string(text),
                                 font_size,
                                 scale,
                                 static_cast<int>(family),
                                 bold,
                                 italic,
                                 (to_u8(c.a) << 24) | (to_u8(c.r) << 16) | (to_u8(c.g) << 8) |
                                     to_u8(c.b)};
            auto rit = impl_->raster_cache.find(rkey);
            if (rit == impl_->raster_cache.end()) {
                if (impl_->raster_cache.size() > 256) {
                    impl_->raster_cache.clear();
                }
                rit = impl_->raster_cache
                          .emplace(std::move(rkey),
                                   rasterize(text, font_size, scale, c, family, bold, italic))
                          .first;
            }
            auto const &rast = rit->second;
            if (rast.pixels.empty()) {
                return;
            }

            // Use the neutral AAAA pixels directly and tint with ColorMatrix
            Gdiplus::Bitmap bmp(rast.width, rast.height, rast.width * 4, PixelFormat32bppARGB,
                                (BYTE *)rast.pixels.data());

            Gdiplus::ColorMatrix matrix = {{{c.r, 0, 0, 0, 0},
                                            {0, c.g, 0, 0, 0},
                                            {0, 0, c.b, 0, 0},
                                            {0, 0, 0, c.a, 0},
                                            {0, 0, 0, 0, 1}}};

            Gdiplus::ImageAttributes attrs;
            attrs.SetColorMatrix(&matrix, Gdiplus::ColorMatrixFlagsDefault,
                                 Gdiplus::ColorAdjustTypeBitmap);

            float lw = rast.width / scale;
            float lh = rast.height / scale;
            float la = rast.ascent; // ascent is already logical (divided by scale in rasterize)

            Gdiplus::GraphicsState state = graphics->Save();
            graphics->TranslateTransform(pos.x, pos.y);
            if (orientation == Painter::TextOrientation::VerticalCCW) {
                graphics->RotateTransform(-90.0f);
            } else {
                graphics->RotateTransform(90.0f);
            }
            graphics->DrawImage(&bmp, Gdiplus::RectF(0.0f, -la, lw, lh), 0.0f, 0.0f,
                                (float)rast.width, (float)rast.height, Gdiplus::UnitPixel, &attrs);
            graphics->Restore(state);
        }
    } else {
        // Fallback for non-GDI+ painters: rasterize and draw as image
        // FIXME: we need a way to get the scale from the painter.
        // For now, let's assume 1.0 or find it from the painter if possible.
        float scale = 1.0f; // Placeholder
        auto rast = rasterize(text, font_size, scale, c, family, bold, italic);
        if (rast.pixels.empty()) {
            return;
        }

        // Colorize. ImageData::pixels is B,G,R,A (see image.hpp) -- UNVERIFIED: not
        // compile-checked on Windows, please build-check before trusting.
        auto pixels = std::move(rast.pixels);
        for (int i = 0; i < rast.width * rast.height; ++i) {
            float a = pixels[i * 4 + 3] / 255.0f;
            pixels[i * 4 + 0] = static_cast<uint8_t>(c.b * 255 * a);
            pixels[i * 4 + 1] = static_cast<uint8_t>(c.g * 255 * a);
            pixels[i * 4 + 2] = static_cast<uint8_t>(c.r * 255 * a);
            pixels[i * 4 + 3] = static_cast<uint8_t>(c.a * 255 * a);
        }

        p.draw_image(ImageData{std::move(pixels), rast.width, rast.height},
                     {pos.x, pos.y - rast.ascent});
    }
}

// UNVERIFIED: not compile-checked on Windows, please build-check before trusting. ImageData's
// B,G,R,A now matches GDI+'s native PixelFormat32bppARGB directly, so the ColorMatrix R<->B swap
// this used to need is gone.
namespace {
// Device-resolution bitmap cache.
//
// GDIPainter applies ScaleTransform(s, s), so drawing an image at its natural *logical* size
// makes GDI+ resample it to s*size device pixels on every single draw. Measured on demo_dock:
// 299us per 48x48 icon at s=1.5 versus 34us at s=1.0 -- an 8.8x penalty that is pure repeated
// resampling, since nothing about the image changed between frames.
//
// So when a draw would resample, keep a copy of the image already scaled to device resolution
// and blit that 1:1 instead. The expensive filtering happens once per (image, device size).
//
// This lives at file scope rather than in GDIPainter::Impl because a fresh GDIPainter is
// constructed for every WM_PAINT -- a per-painter cache would be discarded each frame and never
// hit. Keyed on ImageData::id, which is monotonic and never reused, so an entry belonging to a
// destroyed image can never be served for a different one; eviction is therefore only about
// memory. Access is single-threaded (painting happens on the UI thread).
struct ScaledKey {
    uint64_t id = 0;
    int w = 0, h = 0;
    bool operator==(ScaledKey const &o) const { return id == o.id && w == o.w && h == o.h; }
};
struct ScaledKeyHash {
    size_t operator()(ScaledKey const &k) const {
        size_t h = std::hash<uint64_t>{}(k.id);
        h ^= std::hash<int>{}(k.w) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.h) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
using ScaledMap = std::unordered_map<ScaledKey, std::unique_ptr<Gdiplus::Bitmap>, ScaledKeyHash>;
ScaledMap &scaled_image_cache() {
    // Intentionally leaked: the entries are Gdiplus::Bitmap objects, and a function-local static
    // would be destroyed during static teardown -- which happens *after* GdiplusShutdown, so
    // releasing them then is a use-after-shutdown and faults (observed: exit code 0xC0000005).
    // The cache lives for the whole process anyway, and the OS reclaims the memory at exit.
    static auto *m = new ScaledMap();
    return *m;
}

// Returns a bitmap of `image` pre-scaled to dw x dh device pixels, or nullptr if it could not be
// built (in which case the caller falls back to resampling inline).
Gdiplus::Bitmap *get_scaled_bitmap(ImageData const &image, int dw, int dh) {
    auto &cache = scaled_image_cache();
    ScaledKey key{image.id, dw, dh};
    if (auto it = cache.find(key); it != cache.end()) {
        return it->second.get();
    }
    if (cache.size() > 512) {
        cache.clear();
    }
    Gdiplus::Bitmap src(image.width, image.height, image.width * 4, PixelFormat32bppARGB,
                        (BYTE *)image.pixels.data());
    if (src.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    auto dst = std::make_unique<Gdiplus::Bitmap>(dw, dh, PixelFormat32bppARGB);
    if (dst->GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    Gdiplus::Graphics g(dst.get());
    // Left at GDI+'s default interpolation so the scaled result matches what the previous
    // per-frame resample produced; the only change is that it now happens once.
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    g.DrawImage(&src, Gdiplus::RectF(0, 0, (float)dw, (float)dh), 0, 0, (float)image.width,
                (float)image.height, Gdiplus::UnitPixel, nullptr);
    auto *raw = dst.get();
    cache.emplace(key, std::move(dst));
    return raw;
}
} // namespace

void GDIPainter::draw_image(ImageData const &image, Point position) {
    if (image.width <= 0 || image.height <= 0) {
        return;
    }

    auto s = impl_->scale;
    auto x = std::floor(position.x * s) / s;
    auto y = std::floor(position.y * s) / s;

    // When the transform would make GDI+ resample (any scale != 1), draw a pre-scaled copy 1:1
    // instead -- see scaled_image_cache(). At scale 1 the dest already matches the source, so the
    // original direct path is kept: it is both the reference output and already near-optimal.
    auto dw = static_cast<int>(std::lround(image.width * s));
    auto dh = static_cast<int>(std::lround(image.height * s));
    if (dw != image.width || dh != image.height) {
        if (auto *cached = get_scaled_bitmap(image, dw, dh)) {
            Gdiplus::InterpolationMode old_i = impl_->graphics->GetInterpolationMode();
            // The dest rect maps exactly onto the cached bitmap's pixels, so this is a 1:1 blit
            // and nearest-neighbour is an exact copy rather than a quality choice.
            impl_->graphics->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            impl_->graphics->DrawImage(cached,
                                       Gdiplus::RectF(x, y, (float)image.width,
                                                      (float)image.height),
                                       0, 0, (float)dw, (float)dh, Gdiplus::UnitPixel, nullptr);
            impl_->graphics->SetInterpolationMode(old_i);
            return;
        }
    }

    Gdiplus::Bitmap bmp(image.width, image.height, image.width * 4, PixelFormat32bppARGB,
                        (BYTE *)image.pixels.data());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        spdlog::error("draw_image: failed to create GDI+ bitmap (status: {})",
                      (int)bmp.GetLastStatus());
        return;
    }

    impl_->graphics->DrawImage(&bmp, Gdiplus::RectF(x, y, (float)image.width, (float)image.height),
                               0, 0, (float)image.width, (float)image.height, Gdiplus::UnitPixel,
                               nullptr);
}

// UNVERIFIED: not compile-checked on Windows, please build-check before trusting. See draw_image
// above -- no ColorMatrix swap needed anymore.
void GDIPainter::draw_image_scaled(ImageData const &image, Rect const &dest) {
    if (image.width <= 0 || image.height <= 0 || dest.width <= 0 || dest.height <= 0) {
        return;
    }

    float s = impl_->scale;
    float x = std::floor(dest.x * s) / s;
    float y = std::floor(dest.y * s) / s;
    float w = std::ceil((dest.x + dest.width) * s) / s - x;
    float h = std::ceil((dest.y + dest.height) * s) / s - y;

    Gdiplus::Bitmap bmp(image.width, image.height, image.width * 4, PixelFormat32bppARGB,
                        (BYTE *)image.pixels.data());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        spdlog::error("draw_image_scaled: failed to create GDI+ bitmap (status: {})",
                      (int)bmp.GetLastStatus());
        return;
    }

    Gdiplus::SmoothingMode old_s = impl_->graphics->GetSmoothingMode();
    Gdiplus::InterpolationMode old_i = impl_->graphics->GetInterpolationMode();
    impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    impl_->graphics->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);

    impl_->graphics->DrawImage(&bmp, Gdiplus::RectF(x, y, w, h), 0, 0, (float)image.width,
                               (float)image.height, Gdiplus::UnitPixel, nullptr);

    impl_->graphics->SetSmoothingMode(old_s);
    impl_->graphics->SetInterpolationMode(old_i);
}

Icon GDIPainter::capture(Window *window) {
    float scale = window->scale_factor();
    int lw = static_cast<int>(window->size().width);
    int lh = static_cast<int>(window->size().height);
    if (lw <= 0 || lh <= 0) {
        return nullptr;
    }
    int pw = static_cast<int>(std::ceil(lw * scale));
    int ph = static_cast<int>(std::ceil(lh * scale));

    Gdiplus::Bitmap bitmap(pw, ph, PixelFormat32bppARGB);
    Gdiplus::Graphics g(&bitmap);
    g.Clear(Gdiplus::Color(255, 255, 255, 255));

    Win32TextRasterizer rasterizer;
    GDIPainter painter(&g, scale, &rasterizer);
    window->handle_paint(painter);

    auto result = std::make_shared<ImageData>();
    result->width = pw;
    result->height = ph;
    result->channels = 4;
    result->pixels.resize(pw * ph * 4);

    // UNVERIFIED: not compile-checked on Windows, please build-check before trusting. GDI+
    // PixelFormat32bppARGB is B,G,R,A in memory, which now matches ImageData::pixels directly
    // (see image.hpp) -- straight copy, no channel swap.
    Gdiplus::BitmapData data;
    Gdiplus::Rect rect(0, 0, pw, ph);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) ==
        Gdiplus::Ok) {
        uint8_t *src = static_cast<uint8_t *>(data.Scan0);
        std::memcpy(result->pixels.data(), src, static_cast<size_t>(pw) * ph * 4);
        bitmap.UnlockBits(&data);
        return result;
    }
    return nullptr;
}

} // namespace toolkit
