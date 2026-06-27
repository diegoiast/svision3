// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/text_rasterizer.hpp"
#include "toolkit/types.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace toolkit {

namespace text {
class TextShaper;
} // namespace text

bool platformNeedsCSD();

class Window;
class PlatformApplication;
class PlatformWindow;

class RenderingBackend {
  public:
    virtual ~RenderingBackend() = default;
    virtual std::string_view name() const = 0;
    virtual void paint(Window *owner, PlatformWindow *window, PlatformApplication *app, int lw,
                       int lh) = 0;
    virtual void render_to_buffer(PlatformApplication *app, int w, int h, float scale, void *dst,
                                  std::function<void(Painter &)> fn) = 0;
};

class PlatformWindow {
  public:
    virtual ~PlatformWindow() = default;
    virtual void show() = 0;
    virtual void close() = 0;
    virtual void minimize() = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;
    virtual void set_size(Size s) = 0;
    virtual void request_redraw() = 0;
    virtual void set_min_size(Size s) = 0;
    virtual void set_max_size(Size s) = 0;
    // FIXME: timers should be platform and windows APIs
    virtual int start_timer(float interval_sec, std::function<void()> callback, bool repeats) = 0;
    // FIXME: timers should be platform and windows APIs
    virtual void stop_timer(int timer_id) = 0;
    virtual void set_cursor(CursorShape shape) = 0;
    virtual void show_tooltip_window(std::string const &text, Point pos) = 0;
    virtual void hide_tooltip_window() = 0;
    virtual void start_system_move(uint32_t serial) = 0;
    virtual void start_system_resize(WindowEdge edge, uint32_t serial) = 0;
    virtual void set_modal_for(PlatformWindow *parent) = 0;
    virtual void grab_pointer() = 0;
    virtual void ungrab_pointer() = 0;

    /**
     * @brief Capture the window content to an image.
     * @return A shared pointer to the captured image data.
     */
    virtual Icon capture() = 0;
    virtual float scale_factor() const = 0;

    virtual std::string_view painter_name() const = 0;
    virtual void set_title(std::string_view) {}
    virtual void set_icon(Icon const &icon) = 0;
    virtual Icon get_icon() = 0;
    virtual void show_system_menu(Point p) = 0;
};

class PlatformApplication {
  public:
    virtual ~PlatformApplication() = default;
    virtual std::unique_ptr<PlatformWindow> create_window(std::string_view title, Size size,
                                                          Window *owner, WindowOptions options) = 0;
    virtual std::shared_ptr<ImageLoaderInterface> get_image_loader() = 0;
    virtual std::shared_ptr<SVGLoaderInterface> get_svg_loader() = 0;
    virtual int run() = 0;
    virtual void run_until(std::function<bool()> should_exit) = 0;
    virtual void quit() = 0;
    virtual void post_to_main_thread(std::function<void()> fn) = 0;
    virtual std::string clipboard_get_text() = 0;
    virtual void clipboard_set_text(std::string const &text) = 0;

    void set_rasterizer(TextRasterizer *r) { rasterizer_ = r; }
    TextRasterizer *rasterizer() const { return rasterizer_; }

    void set_shaper(text::TextShaper *s) { shaper_ = s; }
    text::TextShaper *shaper() const { return shaper_; }

    Size measure_text(std::string_view text, float font_size,
                      FontFamily font = FontFamily::System,
                      bool bold = false, bool italic = false) const {
        if (rasterizer_) {
            return rasterizer_->measure(text, font_size, font, bold, italic);
        }
        return {};
    }
    Painter::FontMetrics font_metrics(float font_size, FontFamily font = FontFamily::System) const {
        if (rasterizer_) {
            return rasterizer_->metrics(font_size, font);
        }
        return {};
    }

    virtual std::string_view name() const = 0;
    virtual float scale_factor() const = 0;
    virtual SystemFonts system_fonts() const = 0;

  protected:
    TextRasterizer *rasterizer_ = nullptr;
    text::TextShaper *shaper_ = nullptr;
};

std::unique_ptr<PlatformApplication> create_platform_application();

namespace detail {
PlatformApplication *current_platform();
void set_current_platform(PlatformApplication *p);
} // namespace detail

} // namespace toolkit
