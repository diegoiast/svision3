// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/painters/gl_offscreen.hpp"
#include "toolkit/platform.hpp"

// Inline definition of the connector from wayland to cairo
// No real need for a CPP file as we are includeing this once.

namespace toolkit {

class WaylandlOpenGlBackend : public RenderingBackend {
  public:
    WaylandlOpenGlBackend(wl_surface *surface, Size size, PlatformApplication *pApp) {
        auto app = static_cast<WaylandPlatformApplication *>(pApp);
        auto pw = static_cast<int>(size.width) * app->output_scale;
        auto ph = static_cast<int>(size.height) * app->output_scale;
        egl_window = wl_egl_window_create(surface, pw, ph);
        egl_surface = eglCreateWindowSurface(app->egl_display, app->egl_config,
                                             (EGLNativeWindowType)egl_window, nullptr);
        buf_width = pw;
        buf_height = ph;
    }

    ~WaylandlOpenGlBackend() override {
        if (egl_surface) {
            eglDestroySurface(nullptr, egl_surface);
        }
        if (egl_window) {
            wl_egl_window_destroy(egl_window);
        }
    }

    std::string_view name() const override { return "OpenGL"; }

    void render_to_buffer(PlatformApplication *app, int w, int h, float scale, void *dst,
                          std::function<void(Painter &)> fn) override {
        auto *wl_app = static_cast<WaylandPlatformApplication *>(app);
        eglMakeCurrent(wl_app->egl_display, egl_surface, egl_surface, wl_app->egl_context);
        gl_render_to_buffer(w, h, scale, &rasterizer_, dst, fn);
    }

    void paint(Window *owner, PlatformWindow *pWindow, PlatformApplication *pApp, int lw,
               int lh) override {
        auto window = static_cast<WaylandPlatformWindow *>(pWindow);
        auto app = static_cast<WaylandPlatformApplication *>(pApp);
        auto pw = static_cast<int>(std::ceil(static_cast<float>(lw) * window->scale));
        auto ph = static_cast<int>(std::ceil(static_cast<float>(lh) * window->scale));

        eglMakeCurrent(app->egl_display, egl_surface, egl_surface, app->egl_context);
        if (pw != buf_width || ph != buf_height) {
            wl_egl_window_resize(egl_window, pw, ph, 0, 0);
            buf_width = pw;
            buf_height = ph;
        }

        if (window->viewport) {
            wp_viewport_set_destination(window->viewport, lw, lh);
            wl_surface_set_buffer_scale(window->surface, 1);
        } else {
            wl_surface_set_buffer_scale(window->surface,
                                        static_cast<int32_t>(std::ceil(window->scale)));
        }

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

        // FIXME: do not use cairo inside opengl.
        GLPainter painter(static_cast<float>(lh), window->scale, &rasterizer_);
        owner->handle_paint(painter);

        eglSwapBuffers(app->egl_display, egl_surface);
    }

  private:
    wl_egl_window *egl_window = nullptr;
    void *egl_surface = nullptr;
    int buf_width = 0, buf_height = 0;
    CairoTextRasterizer rasterizer_;
};

} // namespace toolkit