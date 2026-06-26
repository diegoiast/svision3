// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/text/text_layout.hpp"
#include <memory>

namespace toolkit {

// HarfBuzz + FreeType backed TextShaper for the Cairo/X11/Wayland backend.
// Shaping is used purely to get script-correct glyph forms and pixel
// advances; the run's direction is forced from the `rtl` flag the caller
// already resolved via bidi::BidiLine, mirroring win32_shaper.cpp -- this
// class never re-derives bidi ordering on its own. See
// docs/design/rtl-line-input.md section 6 (Cairo backend) and
// docs/design/cairo-shaper-handoff.md for implementation status, known
// gaps, and a verification checklist (this was written without access to a
// Linux/Cairo/HarfBuzz toolchain -- it has never been compiled).
class CairoShaper : public text::TextShaper {
  public:
    CairoShaper();
    ~CairoShaper() override;
    void release_fonts();

    std::vector<text::ClusterAdvance> shape_run(std::string_view run_utf8, bool rtl,
                                                float font_size, FontFamily font) override;
    void draw_run(Painter &painter, std::string_view run_utf8, bool rtl, Point origin,
                 Color const &color, float font_size, FontFamily font) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace toolkit
