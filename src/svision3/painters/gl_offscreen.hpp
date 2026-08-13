// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/text_rasterizer.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace svision3 {

class Painter;

// Render into an offscreen framebuffer object using GLPainter.
// An active GL context must be current before calling.
// Returns BGRA pixels, top-to-bottom, w*h*4 bytes. Empty on failure.
// Renders into dst (w*h*4 bytes, BGRA top-to-bottom). Requires an active GL context.
// On failure dst is zeroed (transparent).
void gl_render_to_buffer(int w, int h, float scale, TextRasterizer *rasterizer, void *dst,
                         std::function<void(Painter &)> fn);

} // namespace svision3
