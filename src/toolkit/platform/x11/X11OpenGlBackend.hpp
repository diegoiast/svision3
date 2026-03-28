// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/platform.hpp"

#include <GL/gl.h>
#include <GL/glx.h>

// Inline definition of the connector from X11 to OpenGL
// No real need for a CPP file as we are includeing this once.

namespace toolkit {

class X11OpenGlBackend : public RenderingBackend {
  public:
    X11OpenGlBackend(::Window xwindow, GLXContext glx_context)
        : xwindow_(xwindow), glx_context_(glx_context) {}

    ~X11OpenGlBackend() override {}

    std::string_view name() const override { return "OpenGL"; }

    void paint(Window *owner, PlatformWindow *window, PlatformApplication *app, int lw,
               int lh) override {
        if (lw <= 0 || lh <= 0) {
            return;
        }
        auto *x11app = static_cast<X11PlatformApplication *>(app);
        auto scale = app->scale_factor();
        auto pw = static_cast<int>(std::ceil(lw * scale));
        auto ph = static_cast<int>(std::ceil(lh * scale));

        glXMakeCurrent(static_cast<Display *>(x11app->get_display()), xwindow_, glx_context_);
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

        CairoTextRasterizer rasterizer;
        GLPainter painter(static_cast<float>(lh), scale, rasterizer);
        owner->handle_paint(painter);

        glXSwapBuffers(static_cast<Display *>(x11app->get_display()), xwindow_);
    }

  private:
    ::Window xwindow_;
    GLXContext glx_context_;
};

} // namespace toolkit