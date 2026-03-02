#pragma once

#include "toolkit/painter.hpp"
#include "toolkit/painters/gl_painter.hpp"

#ifdef TOOLKIT_HAS_CAIRO
struct _cairo;
using cairo_t = _cairo;

namespace toolkit {

class Window;

class CairoPainter : public Painter {
  public:
    explicit CairoPainter(cairo_t *cr);

    void push_clip(Rect const &rect) override;
    void pop_clip() override;

    void set_line_style(LineStyle style) override;

    void fill_rect(Rect const &rect, Color const &color) override;
    void draw_rect(Rect const &rect, Color const &color,
                   float line_width) override;
    void fill_rounded_rect(Rect const &rect, Color const &color,
                           float radius) override;
    void draw_rounded_rect(Rect const &rect, Color const &color, float radius,
                           float line_width) override;
    void draw_line(Point from, Point to, Color const &color,
                   float line_width) override;
    void fill_circle(Point center, float radius,
                     Color const &color) override;
    void draw_circle(Point center, float radius, Color const &color,
                     float line_width) override;
    void draw_text(std::string_view text, Point position, Color const &color,
                   float font_size, FontFamily font = FontFamily::System) override;
    Size text_size(std::string_view text, float font_size,
                   FontFamily font = FontFamily::System) override;
    FontMetrics font_metrics(float font_size,
                             FontFamily font = FontFamily::System) override;

    std::string_view name() const override { return "Cairo"; }

  private:
    cairo_t *cr_;
};

// Shared helpers for all Cairo-based backends
Size cairo_measure_text(std::string_view text, float font_size,
                        FontFamily font = FontFamily::System);
Painter::FontMetrics cairo_measure_font_metrics(float font_size,
                                                FontFamily font = FontFamily::System);
bool cairo_save_to_png(Window *window, std::string const &path);

class CairoTextRasterizer : public TextRasterizer {
  public:
    RasterizedText rasterize(std::string_view text, float font_size, float scale,
                             FontFamily font = FontFamily::System) override;
    Size measure(std::string_view text, float font_size,
                 FontFamily font = FontFamily::System) override;
    Painter::FontMetrics metrics(float font_size,
                                 FontFamily font = FontFamily::System) override;
};

} // namespace toolkit
#endif
