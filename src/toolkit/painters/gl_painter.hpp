#pragma once

#include "toolkit/painter.hpp"
#include "toolkit/text_rasterizer.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace toolkit {

// Platform-independent OpenGL 2.1 painter.
// Requires a TextRasterizer for text operations and an active GL context.
class GLPainter : public Painter {
  public:
    GLPainter(float viewport_h, float scale, TextRasterizer &rasterizer);

    void push_clip(Rect const &rect) override;
    void pop_clip() override;

    void push_translation(Point p) override;
    void pop_translation() override;

    void set_line_style(Painter::LineStyle style) override;

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
                   float font_size, FontFamily font = FontFamily::System,
                   TextOrientation orientation = TextOrientation::Horizontal) override;
    Size text_size(std::string_view text, float font_size,
                   FontFamily font = FontFamily::System) override;
    FontMetrics font_metrics(float font_size,
                             FontFamily font = FontFamily::System) override;

    std::string_view name() const override { return "OpenGL"; }

  private:
    float vh_;
    float scale_;
    TextRasterizer &rasterizer_;
    std::vector<Rect> clips_;
    std::vector<Point> translations_;
    Painter::LineStyle style_ = Painter::LineStyle::Solid;

    void set_color(Color const &c);
    void apply_line_style();
    void apply_scissor(Rect const &r);
};

} // namespace toolkit
