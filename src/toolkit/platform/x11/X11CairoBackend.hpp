// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/platform.hpp"

// Inline definition of the connector from X11 to Cairo
// No real need for a CPP file as we are includeing this once.

namespace toolkit {

class X11CairoBackend : public RenderingBackend {
  public:
    X11CairoBackend(::Window xwindow) : xwindow_(xwindow) {}

    ~X11CairoBackend() override {
        if (cairo_surface_) {
            cairo_surface_destroy(cairo_surface_);
        }
        if (x11_surface_) {
            cairo_surface_destroy(x11_surface_);
        }
    }

    std::string_view name() const override { return "Cairo"; }

    void paint(Window *owner, PlatformWindow *window, PlatformApplication *app, int lw,
               int lh) override {
        if (lw <= 0 || lh <= 0) {
            return;
        }

        auto *x11app = static_cast<X11PlatformApplication *>(app);
        auto scale = x11app->scale_factor();
        auto pw = static_cast<int>(std::ceil(lw * scale));
        auto ph = static_cast<int>(std::ceil(lh * scale));

        if (last_pw_ != pw || last_ph_ != ph) {
            cairo_surface_t *old_surface = cairo_surface_;
            cairo_surface_ = nullptr;

            if (!x11_surface_) {
                x11_surface_ = cairo_xlib_surface_create(
                    static_cast<Display *>(x11app->get_display()), xwindow_,
                    static_cast<Visual *>(x11app->get_visual()), pw, ph);
            } else {
                cairo_xlib_surface_set_size(x11_surface_, pw, ph);
            }

            if (old_surface && last_pw_ > 0 && last_ph_ > 0) {
                // Quick paint old image to window for immediate feedback
                cairo_t *xcr = cairo_create(x11_surface_);
                cairo_scale(xcr, static_cast<double>(pw) / last_pw_,
                            static_cast<double>(ph) / last_ph_);
                cairo_set_source_surface(xcr, old_surface, 0, 0);
                cairo_paint(xcr);
                cairo_destroy(xcr);
                cairo_surface_flush(x11_surface_);
                XFlush(static_cast<Display *>(x11app->get_display()));

                // Also initialize the new backbuffer with the scaled old image
                cairo_surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
                cairo_t *bcr = cairo_create(cairo_surface_);
                cairo_scale(bcr, static_cast<double>(pw) / last_pw_,
                            static_cast<double>(ph) / last_ph_);
                cairo_set_source_surface(bcr, old_surface, 0, 0);
                cairo_paint(bcr);
                cairo_destroy(bcr);
                cairo_surface_destroy(old_surface);
            } else {
                if (old_surface) {
                    cairo_surface_destroy(old_surface);
                }
                cairo_surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
                cairo_t *bcr = cairo_create(cairo_surface_);
                Color const &bgColor = Theme::current().palette.window;
                cairo_set_source_rgba(bcr, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
                cairo_paint(bcr);
                cairo_destroy(bcr);
            }
            last_pw_ = pw;
            last_ph_ = ph;
        }

        if (!cairo_surface_ || !x11_surface_) {
            return;
        }

        cairo_t *cr = cairo_create(cairo_surface_);
        cairo_scale(cr, scale, scale);
        CairoPainter painter(cr);
        owner->handle_paint(painter);
        cairo_surface_flush(cairo_surface_);
        cairo_destroy(cr);

        cairo_t *xcr = cairo_create(x11_surface_);
        cairo_set_source_surface(xcr, cairo_surface_, 0, 0);
        cairo_paint(xcr);
        cairo_destroy(xcr);
        cairo_surface_flush(x11_surface_);
        XFlush(static_cast<Display *>(x11app->get_display()));
    }

  private:
    ::Window xwindow_;
    cairo_surface_t *cairo_surface_ = nullptr;
    cairo_surface_t *x11_surface_ = nullptr;
    int last_pw_ = 0;
    int last_ph_ = 0;
};

} // namespace toolkit
