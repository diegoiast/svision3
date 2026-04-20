// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/platform.hpp"

// Inline definition of the connector from wayland to cairo
// No real need for a CPP file as we are includeing this once.

namespace toolkit {

static int create_shm_file(size_t size);

class WaylandCairoBackend : public RenderingBackend {
  public:
    WaylandCairoBackend() = default;
    ~WaylandCairoBackend() override = default;

    std::string_view name() const override { return "Cairo"; }

    void render_to_buffer(PlatformApplication *, int w, int h, float scale, void *dst,
                          std::function<void(Painter &)> fn) override {
        cairo_surface_t *surf = cairo_image_surface_create_for_data(
            static_cast<unsigned char *>(dst), CAIRO_FORMAT_ARGB32, w, h, w * 4);
        cairo_t *cr = cairo_create(surf);
        cairo_scale(cr, scale, scale);
        CairoPainter painter(cr);
        fn(painter);
        cairo_surface_flush(surf);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }

    void paint(Window *owner, PlatformWindow *pWindow, PlatformApplication *pApp, int lw,
               int lh) override {
        auto window = static_cast<WaylandPlatformWindow *>(pWindow);
        auto app = static_cast<WaylandPlatformApplication *>(pApp);
        auto pw = static_cast<int>(std::ceil(static_cast<float>(lw) * window->scale));
        auto ph = static_cast<int>(std::ceil(static_cast<float>(lh) * window->scale));

        if (pw != window->buf_width || ph != window->buf_height) {
            if (window->buffer) {
                wl_buffer_destroy(window->buffer);
                window->buffer = nullptr;
            }
            if (window->shm_data && window->shm_size > 0) {
                munmap(window->shm_data, window->shm_size);
                window->shm_data = nullptr;
            }
            if (window->shm_fd >= 0) {
                ::close(window->shm_fd);
                window->shm_fd = -1;
            }

            int stride = pw * 4;
            size_t total = static_cast<size_t>(stride) * static_cast<size_t>(ph);
            window->shm_fd = create_shm_file(total);
            if (window->shm_fd < 0) {
                return;
            }
            window->shm_data =
                mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, window->shm_fd, 0);
            if (window->shm_data == MAP_FAILED) {
                window->shm_data = nullptr;
                ::close(window->shm_fd);
                window->shm_fd = -1;
                return;
            }
            window->shm_size = total;
            wl_shm_pool *pool =
                wl_shm_create_pool(app->shm, window->shm_fd, static_cast<int32_t>(total));
            window->buffer =
                wl_shm_pool_create_buffer(pool, 0, pw, ph, stride, WL_SHM_FORMAT_ARGB8888);
            wl_shm_pool_destroy(pool);
            window->buf_width = pw;
            window->buf_height = ph;
        }

        if (window->viewport) {
            wp_viewport_set_destination(window->viewport, lw, lh);
            wl_surface_set_buffer_scale(window->surface, 1);
        } else {
            wl_surface_set_buffer_scale(window->surface,
                                        static_cast<int32_t>(std::ceil(window->scale)));
        }

        if (window->shm_data) {
            int stride = pw * 4;
            cairo_surface_t *cs =
                cairo_image_surface_create_for_data(static_cast<unsigned char *>(window->shm_data),
                                                    CAIRO_FORMAT_ARGB32, pw, ph, stride);
            cairo_t *cr = cairo_create(cs);
            cairo_scale(cr, static_cast<double>(window->scale), static_cast<double>(window->scale));

            CairoPainter painter(cr);
            owner->handle_paint(painter);

            cairo_surface_flush(cs);
            cairo_destroy(cr);
            cairo_surface_destroy(cs);

            wl_surface_attach(window->surface, window->buffer, 0, 0);
            wl_surface_damage(window->surface, 0, 0, pw, ph);
            wl_surface_commit(window->surface);
        }
    }
};

} // namespace toolkit