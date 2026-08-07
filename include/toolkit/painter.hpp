// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"
#include "toolkit/types.hpp"
#include <string_view>
#include <vector>

namespace toolkit {

class TextRasterizer;
struct Palette;

class Painter {
  public:
    explicit Painter(TextRasterizer *rasterizer = nullptr) : rasterizer_(rasterizer) {}
    virtual ~Painter() = default;

    // FIXME: this should be in the rasterizer, not in the painter
    struct FontMetrics {
        float ascent = 0;
        float descent = 0;
        float height = 0; // line_spacing: ascent + descent + line gap
        float cell_height() const { return ascent + descent; }
    };

    enum class LineStyle { Solid, Dashed, Dotted };

    enum class TextOrientation {
        Horizontal,
        VerticalCCW, // 90 degrees counter-clockwise (bottom to top)
        VerticalCW   // 90 degrees clockwise (top to bottom)
    };

    virtual void push_clip(Rect const &rect) = 0;
    virtual void push_clip(Rect const &rect, float radius) { push_clip(rect); }
    virtual void pop_clip() = 0;

    virtual void push_translation(Point p) = 0;
    virtual void pop_translation() = 0;

    virtual void push_rotation(float degrees) = 0;
    virtual void pop_rotation() = 0;

    virtual void set_line_style(LineStyle style) = 0;

    virtual void fill_rect(Rect const &rect, Color const &color) = 0;
    virtual void draw_rect(Rect const &rect, Color const &color, float line_width = 1.0f) = 0;
    virtual void fill_rounded_rect(Rect const &rect, Color const &color, float radius) = 0;
    virtual void draw_rounded_rect(Rect const &rect, Color const &color, float radius,
                                   float line_width = 1.0f) = 0;
    virtual void fill_triangle(Point a, Point b, Point c, Color const &color) = 0;
    // Fills a simple (non-self-intersecting) polygon, convex or concave. Default
    // implementation triangulates via fill_triangle(); backends with a native
    // path-fill API (cairo, GDI+, CoreGraphics) should override for quality/perf.
    virtual void fill_polygon(std::vector<Point> const &points, Color const &color);
    virtual void draw_line(Point from, Point to, Color const &color, float line_width = 1.0f) = 0;
    // Strokes a connected line strip. Default implementation is a loop of
    // draw_line(); backends with a native multi-point path API (cairo, GDI+,
    // CoreGraphics) should override -- a single stroked path gives cleaner
    // joins than independently-capped segments.
    virtual void draw_polyline(std::vector<Point> const &points, Color const &color,
                               float line_width = 1.0f);
    virtual void fill_circle(Point center, float radius, Color const &color) = 0;
    virtual void draw_circle(Point center, float radius, Color const &color,
                             float line_width = 1.0f) = 0;
    // FIXME: do we need to use string_view? all platforms will create a string anyway
    void draw_text(std::string_view text, Point position, Color const &color,
                   float font_size = 14.0f, FontFamily font = FontFamily::System,
                   TextOrientation orientation = TextOrientation::Horizontal, bool bold = false,
                   bool italic = false);
    void draw_mnemonic_text(std::string_view raw_text, Point position, Color const &color,
                            float font_size = 14.0f);
    virtual void draw_image(ImageData const &image, Point position) = 0;
    virtual void draw_image_scaled(ImageData const &image, Rect const &dest) = 0;
    virtual std::string_view name() const = 0;
    virtual float scale_factor() const { return 1.0f; }

    Size measure_text(std::string_view text, float font_size = 14.0f,
                      FontFamily font = FontFamily::System,
                      bool bold = false, bool italic = false);
    FontMetrics font_metrics(float font_size, FontFamily font = FontFamily::System);

    // FIXME: draw_filled_frame - this should be removed and use the version from the theme
    void draw_filled_frame(Rect const &rect, Color bg, Color border, const Palette &palette,
                           bool sunken = false, bool bottom_shadow = false);

    // FIXME: draw_focus_ring - this should be removed and use the version from the theme
    void draw_focus_ring(Rect const &rect, float corner_radius);

    static float snap_to_pixel(float val, float scale);

  protected:
    TextRasterizer *rasterizer_ = nullptr;
};

// Used for testing
class MockPainter : public Painter {
  public:
    MockPainter(TextRasterizer *r = nullptr) { rasterizer_ = r; }
    void push_clip(Rect const &) override {}
    void pop_clip() override {}
    void push_translation(Point) override {}
    void pop_translation() override {}
    void push_rotation(float) override {}
    void pop_rotation() override {}
    void set_line_style(LineStyle) override {}
    void fill_rect(Rect const &, Color const &) override {}
    void draw_rect(Rect const &, Color const &, float) override {}
    void fill_rounded_rect(Rect const &, Color const &, float) override {}
    void draw_rounded_rect(Rect const &, Color const &, float, float) override {}
    void fill_triangle(Point, Point, Point, Color const &) override {}
    void draw_line(Point, Point, Color const &, float) override {}
    void fill_circle(Point, float, Color const &) override {}
    void draw_circle(Point, float, Color const &, float) override {}
    void draw_image(ImageData const &, Point) override {}
    void draw_image_scaled(ImageData const &, Rect const &) override {}
    std::string_view name() const override { return "mock"; }
};

} // namespace toolkit
