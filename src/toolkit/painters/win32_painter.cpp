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
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <string>
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

    HFONT create_font(float font_size, float scale = 1.0f, FontFamily family = FontFamily::System) {
        auto const &t = Theme::current();
        std::string face_name =
            (family == FontFamily::Monospace) ? t.palette.fonts.monospace : t.palette.fonts.system;
        auto wface = to_wide(face_name);
        int height = -static_cast<int>(std::round(font_size * scale));
        DWORD pitch =
            (family == FontFamily::Monospace) ? FIXED_PITCH | FF_MODERN : DEFAULT_PITCH | FF_SWISS;
        return CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, pitch,
                           wface.c_str());
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
                                              FontFamily font) {
    auto wtext = to_wide(text);
    if (wtext.empty()) {
        return {};
    }

    HFONT hfont = impl_->create_font(font_size, scale, font);
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

    for (int i = 0; i < w * h; i++) {
        uint8_t b = src[i * 4 + 0];
        uint8_t g = src[i * 4 + 1];
        uint8_t r = src[i * 4 + 2];
        uint8_t a = static_cast<uint8_t>((r + g + b) / 3);
        result.pixels[i * 4 + 0] = a;
        result.pixels[i * 4 + 1] = a;
        result.pixels[i * 4 + 2] = a;
        result.pixels[i * 4 + 3] = a;
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

    auto cache_key = std::string(text) + '\0'
                   + std::to_string(font_size) + '\0'
                   + (font == FontFamily::Monospace ? '1' : '0');
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

Painter::FontMetrics Win32TextRasterizer::metrics(float font_size, FontFamily font) {
    if (impl_->cached_metrics.has_value() &&
        impl_->cached_metrics->font_size == font_size &&
        impl_->cached_metrics->family == font) {
        return impl_->cached_metrics->metrics;
    }

    HFONT hfont = impl_->create_font(font_size, 1.0f, font);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    TEXTMETRICW tm;
    GetTextMetricsW(impl_->hdc, &tm);

    SelectObject(impl_->hdc, old_font);
    DeleteObject(hfont);

    float ascent = static_cast<float>(tm.tmAscent);
    float descent = static_cast<float>(tm.tmDescent);
    auto result = Painter::FontMetrics{ascent, descent, ascent + descent};
    impl_->cached_metrics = {font_size, font, result};
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

void GDIPainter::push_clip(Rect const &r) {
    impl_->state_stack.push_back(impl_->graphics->Save());
    impl_->graphics->SetClip(Gdiplus::RectF(r.x, r.y, r.width, r.height),
                             Gdiplus::CombineModeIntersect);
}

void GDIPainter::pop_clip() {
    if (!impl_->state_stack.empty()) {
        impl_->graphics->Restore(impl_->state_stack.back());
        impl_->state_stack.pop_back();
    }
}

void GDIPainter::push_translation(Point p) {
    impl_->state_stack.push_back(impl_->graphics->Save());
    impl_->graphics->TranslateTransform(p.x, p.y);
}

void GDIPainter::pop_translation() {
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
    Gdiplus::SolidBrush brush(to_gdiplus_color(c));
    impl_->graphics->FillRectangle(&brush, r.x, r.y, r.width, r.height);
}

void GDIPainter::draw_rect(Rect const &r, Color const &c, float lw) {
    Gdiplus::Pen pen(to_gdiplus_color(c), lw);
    apply_line_style(pen, impl_->line_style, lw);
    impl_->graphics->DrawRectangle(&pen, r.x, r.y, r.width, r.height);
}

void GDIPainter::fill_rounded_rect(Rect const &r, Color const &c, float rad) {
    if (rad <= 0) {
        fill_rect(r, c);
        return;
    }
    Gdiplus::GraphicsPath path;
    float d = rad * 2;
    path.AddArc(r.x, r.y, d, d, 180, 90);
    path.AddArc(r.x + r.width - d, r.y, d, d, 270, 90);
    path.AddArc(r.x + r.width - d, r.y + r.height - d, d, d, 0, 90);
    path.AddArc(r.x, r.y + r.height - d, d, d, 90, 90);
    path.CloseFigure();
    Gdiplus::SolidBrush brush(to_gdiplus_color(c));
    impl_->graphics->FillPath(&brush, &path);
}

void GDIPainter::draw_rounded_rect(Rect const &r, Color const &c, float rad, float lw) {
    if (rad <= 0) {
        draw_rect(r, c, lw);
        return;
    }
    Gdiplus::GraphicsPath path;
    float d = rad * 2;
    path.AddArc(r.x, r.y, d, d, 180, 90);
    path.AddArc(r.x + r.width - d, r.y, d, d, 270, 90);
    path.AddArc(r.x + r.width - d, r.y + r.height - d, d, d, 0, 90);
    path.AddArc(r.x, r.y + r.height - d, d, d, 90, 90);
    path.CloseFigure();
    Gdiplus::Pen pen(to_gdiplus_color(c), lw);
    apply_line_style(pen, impl_->line_style, lw);
    impl_->graphics->DrawPath(&pen, &path);
}

void GDIPainter::fill_triangle(Point a, Point b, Point c, Color const &color) {
    Gdiplus::PointF pts[3] = {{a.x, a.y}, {b.x, b.y}, {c.x, c.y}};
    Gdiplus::SolidBrush brush(to_gdiplus_color(color));
    impl_->graphics->FillPolygon(&brush, pts, 3);
}

void GDIPainter::draw_line(Point a, Point b, Color const &c, float lw) {
    Gdiplus::Pen pen(to_gdiplus_color(c), lw);
    apply_line_style(pen, impl_->line_style, lw);
    impl_->graphics->DrawLine(&pen, a.x, a.y, b.x, b.y);
}

void GDIPainter::fill_circle(Point center, float radius, Color const &c) {
    Gdiplus::SolidBrush brush(to_gdiplus_color(c));
    impl_->graphics->FillEllipse(&brush, center.x - radius, center.y - radius, radius * 2,
                                 radius * 2);
}

void GDIPainter::draw_circle(Point center, float radius, Color const &c, float lw) {
    Gdiplus::Pen pen(to_gdiplus_color(c), lw);
    apply_line_style(pen, impl_->line_style, lw);
    impl_->graphics->DrawEllipse(&pen, center.x - radius, center.y - radius, radius * 2,
                                 radius * 2);
}

void GDIPainter::draw_text(std::string_view text, Point pos, Color const &c, float font_size,
                           FontFamily family, TextOrientation orientation) {
    if (!rasterizer_ || text.empty()) {
        return;
    }
    auto rast = rasterizer_->rasterize(text, font_size, impl_->scale, family);
    if (rast.pixels.empty()) {
        return;
    }
    ImageData img;
    img.pixels = std::move(rast.pixels);
    img.width = rast.width;
    img.height = rast.height;

    // colorize: the rasterizer produces white-on-black alpha mask, apply the requested color
    for (int i = 0; i < img.width * img.height; ++i) {
        float a = img.pixels[i * 4 + 3] / 255.0f;
        img.pixels[i * 4 + 0] = static_cast<uint8_t>(c.r * 255 * a);
        img.pixels[i * 4 + 1] = static_cast<uint8_t>(c.g * 255 * a);
        img.pixels[i * 4 + 2] = static_cast<uint8_t>(c.b * 255 * a);
        img.pixels[i * 4 + 3] = static_cast<uint8_t>(c.a * 255 * a);
    }

    auto draw_at = Point{pos.x, pos.y - rast.ascent};
    if (orientation == TextOrientation::Horizontal) {
        draw_image(img, draw_at);
    } else {
        Gdiplus::GraphicsState state = impl_->graphics->Save();
        impl_->graphics->TranslateTransform(pos.x, pos.y);
        if (orientation == TextOrientation::VerticalCCW) {
            impl_->graphics->RotateTransform(-90.0f);
        } else {
            impl_->graphics->RotateTransform(90.0f);
        }
        draw_image(img, {0, -rast.ascent});
        impl_->graphics->Restore(state);
    }
}

void GDIPainter::draw_image(ImageData const &image, Point position) {
    if (image.width <= 0 || image.height <= 0) {
        spdlog::error("draw_image: image is empty ({}x{})", image.width, image.height);
        return;
    }

    Gdiplus::Bitmap bmp(image.width, image.height, image.width * 4, PixelFormat32bppARGB,
                        (BYTE *)image.pixels.data());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        spdlog::error("draw_image: failed to create GDI+ bitmap (status: {})",
                      (int)bmp.GetLastStatus());
        return;
    }
    impl_->graphics->DrawImage(&bmp, position.x, position.y, static_cast<float>(image.width),
                               static_cast<float>(image.height));
}

void GDIPainter::draw_image_scaled(ImageData const &image, Rect const &dest) {
    if (image.width <= 0 || image.height <= 0 || dest.width <= 0 || dest.height <= 0) {
        spdlog::error(
            "draw_image_scaled: image or destination is empty (image: {}x{}, dest: {}x{})",
            image.width, image.height, dest.width, dest.height);
        return;
    }
    Gdiplus::Bitmap bmp(image.width, image.height, image.width * 4, PixelFormat32bppARGB,
                        (BYTE *)image.pixels.data());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        spdlog::error("draw_image_scaled: failed to create GDI+ bitmap (status: {})",
                      (int)bmp.GetLastStatus());
        return;
    }
    impl_->graphics->DrawImage(&bmp, dest.x, dest.y, dest.width, dest.height);
}


static int GetEncoderClsid(const WCHAR *format, CLSID *pClsid) {
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return -1;
    }
    Gdiplus::ImageCodecInfo *pImageCodecInfo = (Gdiplus::ImageCodecInfo *)(malloc(size));
    if (pImageCodecInfo == nullptr) {
        return -1;
    }
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

bool GDIPainter::save_to_png(Window *window, std::string const &path) {
    float scale = window->scale_factor();
    int lw = static_cast<int>(window->size().width);
    int lh = static_cast<int>(window->size().height);
    if (lw <= 0 || lh <= 0) {
        return false;
    }
    int pw = static_cast<int>(std::ceil(lw * scale));
    int ph = static_cast<int>(std::ceil(lh * scale));

    Gdiplus::Bitmap bitmap(pw, ph, PixelFormat32bppARGB);
    Gdiplus::Graphics g(&bitmap);
    g.Clear(Gdiplus::Color(255, 255, 255, 255));

    GDIPainter painter(&g, scale);
    window->handle_paint(painter);

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) == -1) {
        return false;
    }
    auto wpath = to_wide(path);
    return bitmap.Save(wpath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
}

} // namespace toolkit

#endif // _WIN32
