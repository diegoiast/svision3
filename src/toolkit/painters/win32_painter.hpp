#pragma once

#include "toolkit/painters/gl_painter.hpp"
#include <memory>
#include <string>

namespace toolkit {

class Window;

class Win32TextRasterizer : public TextRasterizer {
  public:
    Win32TextRasterizer();
    ~Win32TextRasterizer() override;

    RasterizedText rasterize(std::string_view text, float font_size,
                             float scale, FontFamily font = FontFamily::System) override;
    Size measure(std::string_view text, float font_size,
                 FontFamily font = FontFamily::System) override;
    Painter::FontMetrics metrics(float font_size,
                                 FontFamily font = FontFamily::System) override;

      private:
      struct Impl;
      std::unique_ptr<Impl> impl_;
  };
  
  class GDIPainter : public Painter {
    public:
      explicit GDIPainter(void *hdc, float scale = 1.0f);
      ~GDIPainter() override;
        void push_clip(Rect const &rect) override;
      void pop_clip() override;
      void fill_rect(Rect const &rect, Color const &color) override;
      void draw_rect(Rect const &rect, Color const &color, float line_width) override;
      void fill_rounded_rect(Rect const &rect, Color const &color, float radius) override;
      void draw_rounded_rect(Rect const &rect, Color const &color, float radius,
                             float line_width) override;
      void draw_line(Point from, Point to, Color const &color, float line_width) override;
      void fill_circle(Point center, float radius, Color const &color) override;
      void draw_circle(Point center, float radius, Color const &color, float line_width) override;
      void draw_text(std::string_view text, Point position, Color const &color,
                     float font_size, FontFamily font = FontFamily::System) override;
      Size text_size(std::string_view text, float font_size,
                     FontFamily font = FontFamily::System) override;
              FontMetrics font_metrics(float font_size,
                                       FontFamily font = FontFamily::System) override;
          
              static Size measure_text_gdiplus(std::string_view text, float font_size,
                                               FontFamily font = FontFamily::System);
              static FontMetrics font_metrics_gdiplus(float font_size,
                                                      FontFamily font = FontFamily::System);
              static bool save_to_png(Window *window, std::string const &path);
          
              std::string_view name() const override { return "GDI+"; }
          
          private:
      struct Impl;
      std::unique_ptr<Impl> impl_;
  };
  
  } // namespace toolkit
  
