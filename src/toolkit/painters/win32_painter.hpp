#pragma once

#include "toolkit/painters/gl_painter.hpp"
#include <memory>

namespace toolkit {

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

} // namespace toolkit
