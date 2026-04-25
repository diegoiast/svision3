#pragma once

// Inline definition of Win32 rendering backends (OpenGL and GDI+).
// No real need for a CPP file as we are including this once.

// clang-format off
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <assert.h>
#include <windows.h>
// clang-format on

#include "toolkit/painters/gl_offscreen.hpp"
#include "toolkit/painters/gl_painter.hpp"
#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include "win32_platform.hpp"
#include <GL/gl.h>

namespace toolkit {
float get_window_scale(HWND hwnd);

class Win32OpenGlRenderingBackend : public RenderingBackend {
  public:
    Win32OpenGlRenderingBackend(HWND hwnd, HGLRC hglrc) : hwnd_(hwnd), hglrc_(hglrc) {}

    std::string_view name() const override { return "OpenGL"; }

    void render_to_buffer(PlatformApplication *, int w, int h, float scale, void *dst,
                          std::function<void(Painter &)> fn) override {
        HDC hdc = GetDC(hwnd_);
        wglMakeCurrent(hdc, hglrc_);
        gl_render_to_buffer(w, h, scale, &rasterizer_, dst, fn);
        wglMakeCurrent(nullptr, nullptr);
        ReleaseDC(hwnd_, hdc);
    }

    void paint(Window *win, PlatformWindow *window, PlatformApplication *, int lw, int lh) override {
        auto win_plat = static_cast<Win32PlatformWindow *>(window);
        auto scale = get_window_scale(win_plat->hwnd);
        auto pw = static_cast<int>(std::ceil(lw * scale));
        auto ph = static_cast<int>(std::ceil(lh * scale));

        assert(win_plat->hglrc);

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(win_plat->hwnd, &ps);

        wglMakeCurrent(hdc, win_plat->hglrc);
        glViewport(0, 0, pw, ph);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, lw, lh, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLPainter painter(static_cast<float>(lh), scale, &rasterizer_);
        win->handle_paint(painter);

        glFlush();
        SwapBuffers(hdc);
        wglMakeCurrent(nullptr, nullptr);
        EndPaint(win_plat->hwnd, &ps);
    }

  private:
    HWND hwnd_;
    HGLRC hglrc_;
    Win32TextRasterizer rasterizer_;
};

class Win32GDIRenderingBackend : public RenderingBackend {
  public:
    std::string_view name() const override { return "GDI+"; }

    void render_to_buffer(PlatformApplication *, int w, int h, float scale, void *dst,
                          std::function<void(Painter &)> fn) override {
        HDC screen_dc = GetDC(nullptr);
        HDC mem_dc = CreateCompatibleDC(screen_dc);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void *bits = nullptr;
        HBITMAP hbm = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(mem_dc, hbm));
        {
            GDIPainter painter(mem_dc, scale);
            fn(painter);
        }
        std::memcpy(dst, bits, static_cast<size_t>(w) * h * 4);
        SelectObject(mem_dc, old_bm);
        DeleteObject(hbm);
        DeleteDC(mem_dc);
        ReleaseDC(nullptr, screen_dc);
    }

    void paint(Window *win, PlatformWindow *window, PlatformApplication *, int lw, int lh) override {
        auto win_plat = static_cast<Win32PlatformWindow *>(window);
        auto scale = get_window_scale(win_plat->hwnd);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(win_plat->hwnd, &ps);
        {
            GDIPainter painter(hdc, scale);
            win->handle_paint(painter);
        }
        EndPaint(win_plat->hwnd, &ps);
    }
};

} // namespace toolkit
