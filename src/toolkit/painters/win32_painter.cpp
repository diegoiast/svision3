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
// clang-format on

#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <cmath>
#include <cstring>
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

// ── Win32TextRasterizer ──────────────────────────────────────────────────────

struct Win32TextRasterizer::Impl {
    HDC hdc = nullptr;

    struct MetricsCache {
        float font_size = -1.0f;
        FontFamily family = FontFamily::System;
        Painter::FontMetrics metrics{};
    };
    std::optional<MetricsCache> cached_metrics;
    std::unordered_map<std::string, Size> measure_cache;

    HFONT create_font(float font_size, float scale = 1.0f, FontFamily family = FontFamily::System,
                      bool bold = false, bool italic = false) {
        auto const &t = Theme::current();
        std::string face_name =
            (family == FontFamily::Monospace) ? t.palette.fonts.monospace : t.palette.fonts.system;
        auto wface = to_wide(face_name);
        int height = -static_cast<int>(std::round(font_size * scale));
        DWORD pitch =
            (family == FontFamily::Monospace) ? FIXED_PITCH | FF_MODERN : DEFAULT_PITCH | FF_SWISS;
        return CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, italic ? TRUE : FALSE,
                           FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, pitch, wface.c_str());
    }
};

Win32TextRasterizer::Win32TextRasterizer() : impl_(std::make_unique<Impl>()) {
    impl_->hdc = CreateCompatibleDC(nullptr);
    SetBkMode(impl_->hdc, TRANSPARENT);
}

Win32TextRasterizer::~Win32TextRasterizer() {
    if (impl_->hdc) {
        DeleteDC(impl_->hdc);
    }
}

RasterizedText Win32TextRasterizer::rasterize(std::string_view text, float font_size, float scale,
                                              Color const &color, FontFamily font, bool bold,
                                              bool italic) {
    auto wtext = to_wide(text);
    if (wtext.empty()) {
        return {};
    }

    HFONT hfont = impl_->create_font(font_size, scale, font, bold, italic);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    SIZE sz;
    GetTextExtentPoint32W(impl_->hdc, wtext.c_str(), static_cast<int>(wtext.size()), &sz);

    int w = sz.cx;
    int h = sz.cy;
    if (w <= 0 || h <= 0) {
        SelectObject(impl_->hdc, old_font);
        DeleteObject(hfont);
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
        DeleteObject(hfont);
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
    DeleteObject(hfont);

    return result;
}

Size Win32TextRasterizer::measure(std::string_view text, float font_size, FontFamily font) {
    if (text.empty()) {
        return {0, 0};
    }

    auto cache_key = std::string(text) + '\0' + std::to_string(font_size) + '\0' +
                     (font == FontFamily::Monospace ? '1' : '0');
    auto it = impl_->measure_cache.find(cache_key);
    if (it != impl_->measure_cache.end()) {
        return it->second;
    }

    auto wtext = to_wide(text);
    HFONT hfont = impl_->create_font(font_size, 1.0f, font);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    SIZE sz;
    GetTextExtentPoint32W(impl_->hdc, wtext.c_str(), static_cast<int>(wtext.size()), &sz);

    SelectObject(impl_->hdc, old_font);
    DeleteObject(hfont);

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

// ── GDIPainter ───────────────────────────────────────────────────────────────

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
    float s = impl_->scale;
    float fx = std::floor(r.x * s);
    float fy = std::floor(r.y * s);
    float lx = std::ceil((r.x + r.width) * s);
    float ly = std::ceil((r.y + r.height) * s);
    float slw = std::max(1.0f, std::round(lw * s));

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

void GDIPainter::fill_rounded_rect(Rect const &r, Color const &c, float rad) {
    if (rad <= 0) {
        fill_rect(r, c);
        return;
    }
    float s = impl_->scale;
    float x = std::floor(r.x * s) / s;
    float y = std::floor(r.y * s) / s;
    float w = std::ceil((r.x + r.width) * s) / s - x;
    float h = std::ceil((r.y + r.height) * s) / s - y;

    Gdiplus::GraphicsPath path;
    float d = rad * 2;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    Gdiplus::SolidBrush brush(to_gdiplus_color(c));
    impl_->graphics->FillPath(&brush, &path);
}

void GDIPainter::draw_rounded_rect(Rect const &r, Color const &c, float rad, float lw) {
    if (rad <= 0) {
        draw_rect(r, c, lw);
        return;
    }
    float s = impl_->scale;
    float slw = std::max(1.0f, std::round(lw * s)) / s;

    float fx = std::floor(r.x * s);
    float fy = std::floor(r.y * s);
    float lx = std::ceil((r.x + r.width) * s);
    float ly = std::ceil((r.y + r.height) * s);

    // For anti-aliased rounded rects, we center the stroke on the outer pixel edge
    float x = (fx + 0.5f) / s;
    float y = (fy + 0.5f) / s;
    float w = (lx - fx - 1.0f) / s;
    float h = (ly - fy - 1.0f) / s;

    Gdiplus::GraphicsPath path;
    float d = rad * 2;
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
    float s = impl_->scale;
    auto snap = [s](Point p) {
        return Gdiplus::PointF(std::floor(p.x * s) / s, std::floor(p.y * s) / s);
    };
    Gdiplus::PointF pts[3] = {snap(a), snap(b), snap(c)};
    Gdiplus::SolidBrush brush(to_gdiplus_color(color));
    impl_->graphics->FillPolygon(&brush, pts, 3);
}

void GDIPainter::draw_line(Point a, Point b, Color const &c, float lw) {
    float s = impl_->scale;
    float slw = std::max(1.0f, std::round(lw * s)) / s;

    bool axis_aligned = (std::abs(a.x - b.x) < 0.001f || std::abs(a.y - b.y) < 0.001f);
    Gdiplus::SmoothingMode old = impl_->graphics->GetSmoothingMode();
    if (axis_aligned) {
        impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    }

    float x1, y1, x2, y2;
    if (axis_aligned) {
        x1 = std::floor(a.x * s) / s;
        y1 = std::floor(a.y * s) / s;
        x2 = std::floor(b.x * s) / s;
        y2 = std::floor(b.y * s) / s;
    } else {
        x1 = (std::floor(a.x * s) + 0.5f) / s;
        y1 = (std::floor(a.y * s) + 0.5f) / s;
        x2 = (std::floor(b.x * s) + 0.5f) / s;
        y2 = (std::floor(b.y * s) + 0.5f) / s;
    }

    Gdiplus::Pen pen(to_gdiplus_color(c), slw);
    apply_line_style(pen, impl_->line_style, slw);
    impl_->graphics->DrawLine(&pen, x1, y1, x2, y2);

    if (axis_aligned) {
        impl_->graphics->SetSmoothingMode(old);
    }
}

void GDIPainter::fill_circle(Point center, float radius, Color const &c) {
    float s = impl_->scale;
    float cx = std::round(center.x * s) / s;
    float cy = std::round(center.y * s) / s;
    Gdiplus::SolidBrush brush(to_gdiplus_color(c));
    impl_->graphics->FillEllipse(&brush, cx - radius, cy - radius, radius * 2, radius * 2);
}

void GDIPainter::draw_circle(Point center, float radius, Color const &c, float lw) {
    float s = impl_->scale;
    float slw = std::max(1.0f, std::round(lw * s)) / s;

    float cx = std::round(center.x * s) / s;
    float cy = std::round(center.y * s) / s;

    Gdiplus::Pen pen(to_gdiplus_color(c), slw);
    apply_line_style(pen, impl_->line_style, slw);
    impl_->graphics->DrawEllipse(&pen, cx - radius, cy - radius, radius * 2, radius * 2);
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
        auto wface = to_wide(face);

        auto *graphics = static_cast<Gdiplus::Graphics *>(gp->graphics());
        auto scale = gp->scale();

        // Build the GDI+ font at font_size logical pixels (UnitPixel).
        // With ScaleTransform(scale) active, GDI+ renders this as font_size*scale physical pixels —
        // identical to what the rasterizer produces, with no manual bitmap scaling needed.
        Gdiplus::FontFamily ff(wface.c_str());
        int gdi_style = Gdiplus::FontStyleRegular;
        if (bold && italic) {
            gdi_style = Gdiplus::FontStyleBoldItalic;
        } else if (bold) {
            gdi_style = Gdiplus::FontStyleBold;
        } else if (italic) {
            gdi_style = Gdiplus::FontStyleItalic;
        }
        Gdiplus::Font font(&ff, font_size, gdi_style, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(to_gdiplus_color(c));

        // GenericTypographic removes GDI+'s default internal margins.
        Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
        format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsNoFitBlackBox |
                              Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

        // Use the rasterizer's metrics ascent so that draw_text aligns with the baseline_y
        // that layout code computes via painter.font_metrics() — both come from the same GDI path.
        auto ascent = metrics(font_size, family).ascent;

        // DrawString places the origin at the top-left of the layout box; pos.y is the baseline.
        if (orientation == Painter::TextOrientation::Horizontal) {
            Gdiplus::PointF draw_at(pos.x, pos.y - ascent);
            graphics->DrawString(wtext.c_str(), -1, &font, draw_at, &format, &brush);
        } else {
            // Rotating the GDI+ context before DrawString disables ClearType (subpixel layout
            // is undefined after rotation), producing pixelated greyscale-AA text.
            // Instead, rasterize the text horizontally (full ClearType quality), colorize the
            // resulting bitmap, then draw it rotated.  At exactly ±90° the pixel mapping is
            // 1-to-1 so there are no interpolation artifacts from the rotation itself.
            auto rast = rasterize(text, font_size, scale, c, family, bold, italic);
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

        // Colorize
        auto pixels = std::move(rast.pixels);
        for (int i = 0; i < rast.width * rast.height; ++i) {
            float a = pixels[i * 4 + 3] / 255.0f;
            pixels[i * 4 + 0] = static_cast<uint8_t>(c.r * 255 * a);
            pixels[i * 4 + 1] = static_cast<uint8_t>(c.g * 255 * a);
            pixels[i * 4 + 2] = static_cast<uint8_t>(c.b * 255 * a);
            pixels[i * 4 + 3] = static_cast<uint8_t>(c.a * 255 * a);
        }

        p.draw_image(ImageData{std::move(pixels), rast.width, rast.height},
                     {pos.x, pos.y - rast.ascent});
    }
}

void GDIPainter::draw_image(ImageData const &image, Point position) {
    if (image.width <= 0 || image.height <= 0) {
        return;
    }

    float s = impl_->scale;
    float x = std::floor(position.x * s) / s;
    float y = std::floor(position.y * s) / s;

    Gdiplus::Bitmap bmp(image.width, image.height, image.width * 4, PixelFormat32bppARGB,
                        (BYTE *)image.pixels.data());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        spdlog::error("draw_image: failed to create GDI+ bitmap (status: {})",
                      (int)bmp.GetLastStatus());
        return;
    }

    Gdiplus::ColorMatrix swapRB = {{
        {0, 0, 1, 0, 0}, // Red output comes from Blue input
        {0, 1, 0, 0, 0}, // Green remains Green
        {1, 0, 0, 0, 0}, // Blue output comes from Red input
        {0, 0, 0, 1, 0}, // Alpha remains Alpha
        {0, 0, 0, 0, 1}  // Dummy
    }};

    Gdiplus::ImageAttributes attrs;
    attrs.SetColorMatrix(&swapRB, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

    impl_->graphics->DrawImage(&bmp, Gdiplus::RectF(x, y, (float)image.width, (float)image.height),
                               0, 0, (float)image.width, (float)image.height, Gdiplus::UnitPixel,
                               &attrs);
}

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

    Gdiplus::ColorMatrix swapRB = {{
        {0, 0, 1, 0, 0}, // Red output comes from Blue input
        {0, 1, 0, 0, 0}, // Green remains Green
        {1, 0, 0, 0, 0}, // Blue output comes from Red input
        {0, 0, 0, 1, 0}, // Alpha remains Alpha
        {0, 0, 0, 0, 1}  // Dummy
    }};

    Gdiplus::ImageAttributes attrs;
    attrs.SetColorMatrix(&swapRB, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

    Gdiplus::SmoothingMode old_s = impl_->graphics->GetSmoothingMode();
    Gdiplus::InterpolationMode old_i = impl_->graphics->GetInterpolationMode();
    impl_->graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    impl_->graphics->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);

    impl_->graphics->DrawImage(&bmp, Gdiplus::RectF(x, y, w, h), 0, 0, (float)image.width,
                               (float)image.height, Gdiplus::UnitPixel, &attrs);

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

    Gdiplus::BitmapData data;
    Gdiplus::Rect rect(0, 0, pw, ph);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) ==
        Gdiplus::Ok) {
        uint8_t *src = static_cast<uint8_t *>(data.Scan0);
        for (int i = 0; i < pw * ph; ++i) {
            // GDI+ PixelFormat32bppARGB is BGRA
            // ImageData is RGBA
            result->pixels[i * 4 + 0] = src[i * 4 + 2]; // R
            result->pixels[i * 4 + 1] = src[i * 4 + 1]; // G
            result->pixels[i * 4 + 2] = src[i * 4 + 0]; // B
            result->pixels[i * 4 + 3] = src[i * 4 + 3]; // A
        }
        bitmap.UnlockBits(&data);
        return result;
    }
    return nullptr;
}

} // namespace toolkit

#endif // _WIN32
