// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace toolkit {

struct RasterizedText {
    std::vector<uint8_t> pixels; // RGBA, top-to-bottom, premultiplied
    int width = 0;               // pixel width
    int height = 0;              // pixel height
    float ascent = 0;            // font ascent in points
};

class TextRasterizer {
  public:
    virtual ~TextRasterizer() = default;

    virtual RasterizedText rasterize(std::string_view text, float font_size, float scale,
                                     FontFamily font = FontFamily::System) = 0;

    // FIXME: those functions seem very similar. We could merge them
    // FIXME: Maybe cache the font metrics, instead of computing them?
    // FIXME: all platforms (win32, cairo) create a font and delete on each call
    //        It would be cool to tell the metric which font to use so they would
    //        cache.
    virtual Size measure(std::string_view text, float font_size,
                         FontFamily font = FontFamily::System) = 0;
    virtual Painter::FontMetrics metrics(float font_size,
                                         FontFamily font = FontFamily::System) = 0;
};

} // namespace toolkit
