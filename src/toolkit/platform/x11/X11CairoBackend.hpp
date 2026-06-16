// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/platform.hpp"
#include <X11/Xlib.h>
#include <cairo-xlib.h>

namespace toolkit {

class X11CairoBackend : public RenderingBackend {
  public:
    X11CairoBackend(::Window xwindow, void *visual)
        : xwindow_(xwindow), visual_(static_cast<Visual *>(visual)) {
        if (!visual_) {
            spdlog::error("X11CairoBackend created with null visual!");
        }
    }

    ~X11CairoBackend() override {
        if (cairo_surface_) {
            cairo_surface_destroy(cairo_surface_);
        }
        if (x11_surface_) {
            cairo_surface_destroy(x11_surface_);
        }
    }

    std::string_view name() const override { return "Cairo"; }

    void render_to_buffer(PlatformApplication *pApp, int w, int h, float scale, void *dst,
                          std::function<void(Painter &)> fn) override {
        cairo_surface_t *surf = cairo_image_surface_create_for_data(
            static_cast<unsigned char *>(dst), CAIRO_FORMAT_ARGB32, w, h, w * 4);
        cairo_t *cr = cairo_create(surf);
        cairo_scale(cr, scale, scale);
        CairoPainter painter(cr, pApp->rasterizer());
        fn(painter);
        cairo_surface_flush(surf);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }

    void paint(Window *owner, PlatformWindow *window, PlatformApplication *app, int lw,
               int lh) override {
        if (lw <= 0 || lh <= 0 || !visual_) {
            return;
        }

        auto *x11app = static_cast<X11PlatformApplication *>(app);
        auto scale = x11app->scale_factor();
        auto pw = static_cast<int>(std::ceil(lw * scale));
        auto ph = static_cast<int>(std::ceil(lh * scale));
        auto dimensions_changed = last_pw_ != pw || last_ph_ != ph;
        auto buffer_too_small = !cairo_surface_ ||
                                cairo_image_surface_get_width(cairo_surface_) < pw ||
                                cairo_image_surface_get_height(cairo_surface_) < ph;

        if (dimensions_changed) {
            if (!x11_surface_) {
                x11_surface_ = cairo_xlib_surface_create(
                    static_cast<Display *>(x11app->get_display()), xwindow_, visual_, pw, ph);
            } else {
                cairo_xlib_surface_set_size(x11_surface_, pw, ph);
            }

            if (cairo_surface_status(x11_surface_) != CAIRO_STATUS_SUCCESS) {
                return;
            }

            if (buffer_too_small) {
                if (cairo_surface_) {
                    cairo_surface_destroy(cairo_surface_);
                }
                // Allocate a slightly larger buffer to reduce future reallocations
                auto alloc_w = (pw + 127) & ~127;
                auto alloc_h = (ph + 127) & ~127;
                cairo_surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, alloc_w, alloc_h);
            }

            if (cairo_surface_status(cairo_surface_) != CAIRO_STATUS_SUCCESS) {
                return;
            }

            last_pw_ = pw;
            last_ph_ = ph;
        }

        if (!cairo_surface_ || !x11_surface_ ||
            cairo_surface_status(x11_surface_) != CAIRO_STATUS_SUCCESS) {
            return;
        }

        auto *cr = cairo_create(cairo_surface_);
        cairo_rectangle(cr, 0, 0, pw, ph);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_fill(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        auto fo = cairo_font_options_create();
        cairo_surface_get_font_options(x11_surface_, fo);
        cairo_set_font_options(cr, fo);
        cairo_font_options_destroy(fo);
        cairo_scale(cr, scale, scale);
        CairoPainter painter(cr, x11app->rasterizer());
        owner->handle_paint(painter);
        cairo_surface_flush(cairo_surface_);
        cairo_destroy(cr);

        auto xcr = cairo_create(x11_surface_);
        if (owner->options().csd) {
            cairo_set_operator(xcr, CAIRO_OPERATOR_SOURCE);
        }
        cairo_set_source_surface(xcr, cairo_surface_, 0, 0);
        cairo_paint(xcr);
        cairo_destroy(xcr);
        cairo_surface_flush(x11_surface_);
        XFlush(static_cast<Display *>(x11app->get_display()));
    }

  private:
    ::Window xwindow_;
    Visual *visual_ = nullptr;
    cairo_surface_t *cairo_surface_ = nullptr;
    cairo_surface_t *x11_surface_ = nullptr;
    int last_pw_ = 0;
    int last_ph_ = 0;
};

} // namespace toolkit
