// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/text/text_layout.hpp"
#include <memory>

namespace toolkit {

// Uniscribe-backed TextShaper. Shaping (ScriptItemize/ScriptShape/ScriptPlace)
// is used purely to get script-correct glyph forms and pixel advances; the
// run's direction is forced from the `rtl` flag the caller already resolved
// via bidi::BidiLine, so Uniscribe never re-derives ordering on its own. See
// docs/design/rtl-line-input.md section 6 (Win32 backend).
class Win32Shaper : public text::TextShaper {
  public:
    Win32Shaper();
    ~Win32Shaper() override;

    // Drops every cached HFONT/SCRIPT_CACHE (keyed by resolved face name +
    // pixel size -- see Impl::font_cache in win32_shaper.cpp). Not called
    // from anywhere yet; the destructor does the same cleanup on its own
    // regardless, so this is an optional early-release hook (e.g. for a
    // future theme-change handler), mirroring CairoShaper::release_fonts().
    void release_fonts();

    std::vector<text::ClusterAdvance> shape_run(std::string_view run_utf8, bool rtl,
                                                float font_size, FontFamily font, bool bold = false,
                                                bool italic = false) override;
    void draw_run(Painter &painter, std::string_view run_utf8, bool rtl, Point origin,
                  Color const &color, float font_size, FontFamily font, bool bold = false,
                  bool italic = false) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace toolkit
