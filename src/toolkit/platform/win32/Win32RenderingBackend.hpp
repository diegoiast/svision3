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
#include <objidl.h>
#include <gdiplus.h>
#include <objidl.h>
// clang-format on

#include "toolkit/painters/win32_painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include "win32_platform.hpp"

namespace toolkit {

float get_window_scale(HWND hwnd);

class Win32RenderingBackend : public RenderingBackend {
  public:
    std::string_view name() const { return "GDI+"; }

    void paint(Window *owner, PlatformWindow *window, PlatformApplication *app, int lw, int lh) {
        auto win_plat = static_cast<Win32PlatformWindow *>(window);
        auto scale = get_window_scale(win_plat->hwnd);
        auto pw = static_cast<int>(std::ceil(lw * scale));
        auto ph = static_cast<int>(std::ceil(lh * scale));

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(win_plat->hwnd, &ps);
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateCompatibleBitmap(hdc, pw, ph);
        HBITMAP old_bm = static_cast<HBITMAP>(SelectObject(mem_dc, hbm));

        {
            GDIPainter painter(mem_dc, scale);
            owner->handle_paint(painter);
        }

        BitBlt(hdc, 0, 0, pw, ph, mem_dc, 0, 0, SRCCOPY);
        SelectObject(mem_dc, old_bm);
        DeleteObject(hbm);
        DeleteDC(mem_dc);

        EndPaint(win_plat->hwnd, &ps);
    }
};

} // namespace toolkit
