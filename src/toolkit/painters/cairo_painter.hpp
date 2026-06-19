// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/painter.hpp"
#include "toolkit/painters/gl_painter.hpp"
#include "toolkit/text_rasterizer.hpp"

#ifdef TOOLKIT_HAS_CAIRO
#include "toolkit/painters/cairo_text_shaper.hpp"
struct _cairo;
using cairo_t = _cairo;

namespace toolkit {

class Window;

class CairoPainter : public Painter {
  public:
    explicit CairoPainter(cairo_t *cr, TextRasterizer *rasterizer = nullptr);

    void push_clip(Rect const &rect) override;
    void push_clip(Rect const &rect, float radius) override;
    void pop_clip() override;

    void push_translation(Point p) override;
    void pop_translation() override;

    void push_rotation(float degrees) override;
    void pop_rotation() override;

    void set_line_style(LineStyle style) override;

    void fill_rect(Rect const &rect, Color const &color) override;
    void draw_rect(Rect const &rect, Color const &color, float line_width) override;
    void fill_rounded_rect(Rect const &rect, Color const &color, float radius) override;
    void draw_rounded_rect(Rect const &rect, Color const &color, float radius,
                           float line_width) override;
    void fill_triangle(Point a, Point b, Point c, Color const &color) override;
    void draw_line(Point from, Point to, Color const &color, float line_width) override;
    void fill_circle(Point center, float radius, Color const &color) override;
    void draw_circle(Point center, float radius, Color const &color, float line_width) override;
    void draw_image(ImageData const &image, Point position) override;
    void draw_image_scaled(ImageData const &image, Rect const &dest) override;

    std::string_view name() const override { return "Cairo"; }

    cairo_t *cairo() const { return cr_; }

  private:
    cairo_t *cr_;
};

// Shared helpers for all Cairo-based backends
Size cairo_measure_text(std::string_view text, float font_size,
                        FontFamily font = FontFamily::System);
Painter::FontMetrics cairo_measure_font_metrics(float font_size,
                                                FontFamily font = FontFamily::System);
Icon cairo_capture(Window *window);

class CairoTextRasterizer : public TextRasterizer {
  public:
    RasterizedText rasterize(std::string_view text, float font_size, float scale,
                             Color const &color, FontFamily font = FontFamily::System,
                             bool bold = false, bool italic = false) override;
    Size measure(std::string_view text, float font_size,
                 FontFamily font = FontFamily::System) override;
    Painter::FontMetrics metrics(float font_size, FontFamily font = FontFamily::System) override;
    void draw_text(Painter &p, std::string_view text, Point position, Color const &color,
                   float font_size, FontFamily font, Painter::TextOrientation orientation,
                   bool bold, bool italic) override;
    std::vector<double> cursor_positions(std::string_view text, float font_size,
                                         FontFamily font = FontFamily::System) override;

  private:
#ifdef TOOLKIT_HAS_TEXT_SHAPER
    TextShaper shaper_;
#endif
};

} // namespace toolkit
#endif
