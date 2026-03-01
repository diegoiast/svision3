#ifdef _WIN32

#include "toolkit/painters/win32_painter.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace toolkit {

static std::wstring to_wide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                  static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        result.data(), len);
    return result;
}

struct Win32TextRasterizer::Impl {
    HDC hdc = nullptr;

    HFONT create_font(float font_size, float scale = 1.0f,
                      FontFamily family = FontFamily::System) {
        int height = -static_cast<int>(std::round(font_size * scale));
        const wchar_t *face = (family == FontFamily::Monospace)
                                  ? L"Consolas"
                                  : L"Segoe UI";
        DWORD pitch = (family == FontFamily::Monospace)
                          ? FIXED_PITCH | FF_MODERN
                          : DEFAULT_PITCH | FF_SWISS;
        return CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, pitch, face);
    }
};

Win32TextRasterizer::Win32TextRasterizer()
    : impl_(std::make_unique<Impl>()) {
    impl_->hdc = CreateCompatibleDC(nullptr);
    SetBkMode(impl_->hdc, TRANSPARENT);
}

Win32TextRasterizer::~Win32TextRasterizer() {
    if (impl_->hdc) DeleteDC(impl_->hdc);
}

TextRasterizer::RasterizedText
Win32TextRasterizer::rasterize(std::string_view text, float font_size,
                               float scale, FontFamily font) {
    auto wtext = to_wide(text);
    if (wtext.empty()) return {};

    HFONT hfont = impl_->create_font(font_size, scale, font);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    SIZE sz;
    GetTextExtentPoint32W(impl_->hdc, wtext.c_str(),
                          static_cast<int>(wtext.size()), &sz);

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
    HBITMAP hbm = CreateDIBSection(impl_->hdc, &bmi, DIB_RGB_COLORS, &bits,
                                   nullptr, 0);
    if (!hbm || !bits) {
        SelectObject(impl_->hdc, old_font);
        DeleteObject(hfont);
        return {};
    }

    HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(impl_->hdc, hbm));

    RECT rc = {0, 0, w, h};
    FillRect(impl_->hdc, &rc,
             static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    SetTextColor(impl_->hdc, RGB(255, 255, 255));
    SetBkMode(impl_->hdc, TRANSPARENT);
    TextOutW(impl_->hdc, 0, 0, wtext.c_str(),
             static_cast<int>(wtext.size()));
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

Size Win32TextRasterizer::measure(std::string_view text, float font_size,
                                  FontFamily font) {
    auto wtext = to_wide(text);
    if (wtext.empty()) return {0, 0};

    HFONT hfont = impl_->create_font(font_size, 1.0f, font);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    SIZE sz;
    GetTextExtentPoint32W(impl_->hdc, wtext.c_str(),
                          static_cast<int>(wtext.size()), &sz);

    SelectObject(impl_->hdc, old_font);
    DeleteObject(hfont);

    return {static_cast<float>(sz.cx), static_cast<float>(sz.cy)};
}

Painter::FontMetrics Win32TextRasterizer::metrics(float font_size,
                                                   FontFamily font) {
    HFONT hfont = impl_->create_font(font_size, 1.0f, font);
    HFONT old_font = static_cast<HFONT>(SelectObject(impl_->hdc, hfont));

    TEXTMETRICW tm;
    GetTextMetricsW(impl_->hdc, &tm);

    SelectObject(impl_->hdc, old_font);
    DeleteObject(hfont);

    float ascent = static_cast<float>(tm.tmAscent);
    float descent = static_cast<float>(tm.tmDescent);
    return {ascent, descent, ascent + descent};
}

} // namespace toolkit

#endif // _WIN32
