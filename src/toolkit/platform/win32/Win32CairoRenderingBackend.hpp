#pragma once

// Inline definition of the connector from Windows GDI+
// No real need for a CPP file as we are includeing this once.

// clang-format off
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// #include <objidl.h>
// #include <gdiplus.h>
// #include <objidl.h>
// clang-format on

#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include "win32_platform.hpp"
#include <cairo.h>

namespace toolkit {

float get_window_scale(HWND hwnd);

class Win32CairoRenderingBackend : public RenderingBackend {
    // RenderingBackend interface
  public:
    std::string_view name() const { return "Cairo"; }

    void paint(Window *owner, PlatformWindow *window, PlatformApplication *app, int lw, int lh) {
        auto win_plat = static_cast<Win32PlatformWindow *>(window);
        auto scale = get_window_scale(win_plat->hwnd);
        auto pw = static_cast<int>(std::ceil(lw * scale));
        auto ph = static_cast<int>(std::ceil(lh * scale));

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(win_plat->hwnd, &ps);

        cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
        cairo_t *cr = cairo_create(surface);
        cairo_scale(cr, scale, scale);
        CairoPainter painter(cr);
        owner->handle_paint(painter);
        cairo_surface_flush(surface);
        unsigned char *data = cairo_image_surface_get_data(surface);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = pw;
        bmi.bmiHeader.biHeight = -ph;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(hdc, 0, 0, pw, ph, 0, 0, 0, ph, data, &bmi, DIB_RGB_COLORS);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        EndPaint(win_plat->hwnd, &ps);
    }
};
