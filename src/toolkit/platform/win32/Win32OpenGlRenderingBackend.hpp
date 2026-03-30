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
#include <assert.h>
#include <windows.h>
// clang-format on

// Needed for painting text
#include <toolkit/painters/win32_painter.hpp>

#include "toolkit/painters/gl_painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include "win32_platform.hpp"
#include <GL/gl.h>

namespace toolkit {
float get_window_scale(HWND hwnd);

class Win32OpenGlRenderingBackend : public RenderingBackend {
  public:
    std::string_view name() const { return "OpenGL"; }

    void paint(Window *win, PlatformWindow *window, PlatformApplication *app, int lw, int lh) {
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

        Win32TextRasterizer rasterizer;
        GLPainter painter(static_cast<float>(lh), scale, rasterizer);
        win->handle_paint(painter);

        glFlush();
        SwapBuffers(hdc);
        wglMakeCurrent(nullptr, nullptr);
        EndPaint(win_plat->hwnd, &ps);
    };
};

} // namespace toolkit
