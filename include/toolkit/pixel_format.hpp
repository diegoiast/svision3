// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace toolkit {

// Every native pixel format this toolkit deals with puts alpha last, so the only real question
// at a loader/painter boundary is red vs. blue order. See ImageData::format in image.hpp.
enum class PixelFormat { RGBA, BGRA };

} // namespace toolkit

namespace toolkit::pixel {

// ImageData::pixels (see image.hpp) is always straight-alpha B,G,R,A bytes per pixel -- the
// native in-memory layout of Cairo, GDI+, lunasvg and Win32 DIBs (all "ARGB32" by name, which on
// a little-endian machine is B,G,R,A byte order). OpenGL and CoreGraphics are the two backends
// with the other native layout (R,G,B,A); stb_image also produces true R,G,B,A. Loaders that sit
// on the R,G,B,A side of that boundary use the helpers below so there's exactly one
// implementation of each conversion rather than one per file.
//
// Painters (CairoPainter, GDIPainter) write into a separate destination buffer every frame
// rather than mutating a possibly-shared/cached ImageData in place, so they keep their own fused
// copy+premultiply helpers instead of using these -- these are for loaders that own a freshly
// constructed buffer.

// Swaps the R and B byte of every pixel in place. RGBA<->BGRA is its own inverse.
inline void swap_rb(uint8_t *pixels, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; i++) {
        std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);
    }
}

// Converts premultiplied alpha to straight alpha, in place. Channel positions are untouched, so
// this composes with swap_rb in either order.
inline void unpremultiply(uint8_t *pixels, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; i++) {
        auto a = pixels[i * 4 + 3];
        if (a == 0 || a == 255) {
            continue;
        }
        for (int c = 0; c < 3; c++) {
            auto &v = pixels[i * 4 + c];
            v = static_cast<uint8_t>(std::min(255, (v * 255) / a));
        }
    }
}

} // namespace toolkit::pixel
