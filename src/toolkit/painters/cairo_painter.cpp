#ifdef TOOLKIT_HAS_CAIRO
#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/text/bidi.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cairo.h>
#include <cmath>
#include <spdlog/spdlog.h>
#include <string>

namespace toolkit {

static void rgba_to_argb32(uint8_t *dst, std::vector<uint8_t> const &src) {
    auto size = src.size() / 4;
    for (auto i = 0; i < size; ++i) {
        auto alpha = src[i * 4 + 3] / 255.0f;
        dst[i * 4 + 0] = static_cast<uint8_t>(src[i * 4 + 2] * alpha);
        dst[i * 4 + 1] = static_cast<uint8_t>(src[i * 4 + 1] * alpha);
        dst[i * 4 + 2] = static_cast<uint8_t>(src[i * 4 + 0] * alpha);
        dst[i * 4 + 3] = src[i * 4 + 3];
    }
}

static void argb32_to_rgba(std::vector<uint8_t> &dst, uint8_t const *src, int pixel_count) {
    dst.resize(static_cast<size_t>(pixel_count) * 4);
    for (auto i = 0; i < pixel_count * 4; i += 4) {
        dst[i + 0] = src[i + 2];
        dst[i + 1] = src[i + 1];
        dst[i + 2] = src[i + 0];
        dst[i + 3] = src[i + 3];
    }
}

static std::string font_name_for(FontFamily f) {
    if (f == FontFamily::Monospace) {
        return Theme::current().palette.fonts.monospace;
    }
    return Theme::current().palette.fonts.system;
}

CairoPainter::CairoPainter(cairo_t *cr, TextRasterizer *rasterizer) : Painter(rasterizer), cr_(cr) {
    auto status = cairo_status(cr_);
    if (status != CAIRO_STATUS_SUCCESS) {
        spdlog::error("CairoPainter created with error: {}", cairo_status_to_string(status));
    }
}

void CairoPainter::push_clip(Rect const &rect) {
    cairo_save(cr_);
    if (rect.width > 0 && rect.height > 0) {
        cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
        cairo_clip(cr_);
    }
}

void CairoPainter::pop_clip() { cairo_restore(cr_); }

void CairoPainter::push_translation(Point p) {
    cairo_save(cr_);
    cairo_translate(cr_, p.x, p.y);
}

void CairoPainter::pop_translation() { cairo_restore(cr_); }

void CairoPainter::push_rotation(float degrees) {
    cairo_save(cr_);
    cairo_rotate(cr_, degrees * M_PI / 180.0);
}

void CairoPainter::pop_rotation() { cairo_restore(cr_); }

void CairoPainter::set_line_style(LineStyle style) {
    if (style == LineStyle::Solid) {
        cairo_set_dash(cr_, nullptr, 0, 0);
    } else if (style == LineStyle::Dashed) {
        double dashes[] = {8.0, 4.0};
        cairo_set_dash(cr_, dashes, 2, 0);
    } else if (style == LineStyle::Dotted) {
        double dashes[] = {2.0, 2.0};
        cairo_set_dash(cr_, dashes, 2, 0);
    }
}

void CairoPainter::fill_rect(Rect const &rect, Color const &color) {
    cairo_new_path(cr_);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(cr_);
}

void CairoPainter::draw_rect(Rect const &rect, Color const &color, float line_width) {
    cairo_new_path(cr_);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_stroke(cr_);
}

static void rounded_rect_path(cairo_t *cr, Rect const &r, float radius) {
    auto rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
    cairo_new_path(cr);
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.width - rad, r.y + rad, rad, -M_PI / 2, 0);
    cairo_arc(cr, r.x + r.width - rad, r.y + r.height - rad, rad, 0, M_PI / 2);
    cairo_arc(cr, r.x + rad, r.y + r.height - rad, rad, M_PI / 2, M_PI);
    cairo_arc(cr, r.x + rad, r.y + rad, rad, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void CairoPainter::push_clip(Rect const &rect, float radius) {
    if (radius <= 0) {
        push_clip(rect);
        return;
    }
    cairo_save(cr_);
    if (rect.width > 0 && rect.height > 0) {
        rounded_rect_path(cr_, rect, radius);
        cairo_clip(cr_);
    }
}

void CairoPainter::fill_rounded_rect(Rect const &rect, Color const &color, float radius) {
    rounded_rect_path(cr_, rect, radius);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_fill(cr_);
}

void CairoPainter::draw_rounded_rect(Rect const &rect, Color const &color, float radius,
                                     float line_width) {
    rounded_rect_path(cr_, rect, radius);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    cairo_stroke(cr_);
}

void CairoPainter::fill_triangle(Point a, Point b, Point c, Color const &color) {
    cairo_new_path(cr_);
    cairo_move_to(cr_, a.x, a.y);
    cairo_line_to(cr_, b.x, b.y);
    cairo_line_to(cr_, c.x, c.y);
    cairo_close_path(cr_);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_fill(cr_);
}

void CairoPainter::draw_line(Point from, Point to, Color const &color, float line_width) {
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    cairo_move_to(cr_, from.x, from.y);
    cairo_line_to(cr_, to.x, to.y);
    cairo_stroke(cr_);
}
void CairoPainter::fill_circle(Point center, float radius, Color const &color) {
    cairo_new_path(cr_);
    cairo_arc(cr_, center.x, center.y, radius, 0, 2 * M_PI);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_fill(cr_);
}

void CairoPainter::draw_circle(Point center, float radius, Color const &color, float line_width) {
    cairo_new_path(cr_);
    cairo_arc(cr_, center.x, center.y, radius, 0, 2 * M_PI);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    cairo_stroke(cr_);
}

void CairoTextRasterizer::draw_text(Painter &p, std::string_view text, Point position,
                                    Color const &color, float font_size, FontFamily font,
                                    Painter::TextOrientation orientation, bool bold, bool italic) {
    if (auto *cp = dynamic_cast<CairoPainter *>(&p)) {
        auto *cr = cp->cairo();
        cairo_new_path(cr);

        if (orientation == Painter::TextOrientation::Horizontal) {
            // Use the shaper for horizontal text: BIDI split + per-script font
            // fallback so all Unicode scripts render with an appropriate font.
            auto base = bidi::detect_base_direction(text);
            auto bidi_line = bidi::BidiLine::analyze(text, base);
            auto x = position.x;
            for (auto const &run : bidi_line.runs_visual()) {
                auto run_text = text.substr(run.start, run.length);
                // Shape first to know the run width, then draw at the right x.
                auto advances = shaper_.shape_run(run_text, run.rtl(), font_size, font, bold, italic);
                auto run_width = 0.0f;
                for (auto const &ca : advances) {
                    run_width += ca.advance;
                }
                shaper_.draw_run(p, run_text, run.rtl(), {x, position.y}, color, font_size, font,
                                 bold, italic);
                x += run_width;
            }
        } else {
            // Rotated text: fall back to cairo toy API (no multi-script support).
            cairo_save(cr);
            cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
            cairo_select_font_face(cr, font_name_for(font).c_str(),
                                   italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
                                   bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, std::round(font_size));
            cairo_move_to(cr, position.x, position.y);
            if (orientation == Painter::TextOrientation::VerticalCCW) {
                cairo_rotate(cr, -M_PI / 2.0);
            } else {
                cairo_rotate(cr, M_PI / 2.0);
            }
            std::string s{text};
            cairo_show_text(cr, s.c_str());
            cairo_restore(cr);
        }

        auto status = cairo_status(cr);
        if (status != CAIRO_STATUS_SUCCESS) {
            spdlog::error("CairoPainter: draw_text error: {}", cairo_status_to_string(status));
        }
    } else {
        // Fallback for non-Cairo painters: rasterize and draw as image
        auto scale = p.scale_factor();
        auto rt = rasterize(text, font_size, scale, color, font, bold, italic);
        if (rt.pixels.empty()) {
            return;
        }

        auto snapped_pos = Point{std::floor(position.x * scale + 0.5f) / scale,
                                 std::floor(position.y * scale + 0.5f) / scale};
        p.push_translation(snapped_pos);
        if (orientation == Painter::TextOrientation::VerticalCCW) {
            p.push_rotation(-90.0f);
        } else if (orientation == Painter::TextOrientation::VerticalCW) {
            p.push_rotation(90.0f);
        }
        p.draw_image(ImageData{std::move(rt.pixels), rt.width, rt.height},
                     {(rt.x_offset - 1.0f) / scale, (-rt.ascent - 1.0f) / scale});
        if (orientation != Painter::TextOrientation::Horizontal) {
            p.pop_rotation();
        }
        p.pop_translation();
    }
}

void CairoPainter::draw_image(ImageData const &image, Point position) {
    if (image.width <= 0 || image.height <= 0) {
        return;
    }
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, image.width, image.height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }
    auto *data = cairo_image_surface_get_data(surface);
    rgba_to_argb32(data, image.pixels);
    cairo_surface_mark_dirty(surface);

    cairo_save(cr_);
    cairo_rectangle(cr_, position.x, position.y, image.width, image.height);
    cairo_clip(cr_);
    cairo_set_source_surface(cr_, surface, position.x, position.y);
    cairo_paint(cr_);
    cairo_restore(cr_);
    cairo_surface_destroy(surface);
}

void CairoPainter::draw_image_scaled(ImageData const &image, Rect const &dest) {
    if (image.width <= 0 || image.height <= 0 || dest.width <= 0 || dest.height <= 0) {
        return;
    }
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, image.width, image.height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }
    auto *data = cairo_image_surface_get_data(surface);
    rgba_to_argb32(data, image.pixels);
    cairo_surface_mark_dirty(surface);

    cairo_save(cr_);
    cairo_rectangle(cr_, dest.x, dest.y, dest.width, dest.height);
    cairo_clip(cr_);
    cairo_translate(cr_, dest.x, dest.y);
    cairo_scale(cr_, static_cast<double>(dest.width) / image.width,
                static_cast<double>(dest.height) / image.height);
    cairo_set_source_surface(cr_, surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr_), CAIRO_FILTER_BILINEAR);
    cairo_paint(cr_);
    cairo_restore(cr_);

    cairo_surface_destroy(surface);
}

Icon cairo_capture(Window *window) {
    auto scale = window->scale_factor();
    auto lw = static_cast<int>(window->size().width);
    auto lh = static_cast<int>(window->size().height);
    if (lw <= 0 || lh <= 0) {
        return nullptr;
    }
    auto pw = static_cast<int>(std::ceil(lw * scale));
    auto ph = static_cast<int>(std::ceil(lh * scale));
    auto surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    auto cr = cairo_create(surf);
    auto painter = CairoPainter(cr);

    cairo_scale(cr, scale, scale);
    window->handle_paint(painter);
    cairo_surface_flush(surf);

    auto data = cairo_image_surface_get_data(surf);
    auto result = std::make_shared<ImageData>();
    result->width = pw;
    result->height = ph;
    result->channels = 4;
    argb32_to_rgba(result->pixels, data, pw * ph);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return result;
}

RasterizedText CairoTextRasterizer::rasterize(std::string_view text, float font_size, float scale,
                                              Color const &color, FontFamily font, bool bold,
                                              bool italic) {
    if (text.empty()) {
        return {};
    }

    // Use the shaper (BIDI split + per-script font fallback via fontconfig
    // charset matching) for both sizing and drawing, same as the CairoPainter
    // path in draw_text(). A single cairo_select_font_face()/cairo_show_text()
    // call only ever uses one font, so scripts that font doesn't cover (e.g.
    // Hebrew when the system font is Latin-only) rasterized as .notdef boxes.
    auto base = bidi::detect_base_direction(text);
    auto bidi_line = bidi::BidiLine::analyze(text, base);
    auto runs = bidi_line.runs_visual();

    auto width = 0.0f;
    std::vector<float> run_widths;
    run_widths.reserve(runs.size());
    for (auto const &run : runs) {
        auto run_text = text.substr(run.start, run.length);
        auto run_width = 0.0f;
        for (auto const &ca : shaper_.shape_run(run_text, run.rtl(), font_size, font, bold, italic)) {
            run_width += ca.advance;
        }
        run_widths.push_back(run_width);
        width += run_width;
    }

    auto fm = metrics(font_size, font);

    // Add 2px padding (1px on each side) to avoid clipping and aliasing artifacts
    auto pw = static_cast<int>(std::ceil(width * scale)) + 2;
    auto ph = static_cast<int>(std::ceil(fm.height * scale)) + 2;
    if (pw <= 0 || ph <= 0) {
        return {};
    }

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    cairo_t *cr = cairo_create(surf);

    // Ensure surface is transparent
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_font_options_t *fo = cairo_font_options_create();
    cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_SLIGHT);
    cairo_set_font_options(cr, fo);
    cairo_font_options_destroy(fo);
    cairo_scale(cr, scale, scale);

    // Align ink to the left edge of our surface, plus 1px padding (logical)
    auto pad = 1.0f / scale;
    auto temp_painter = CairoPainter(cr);
    auto x = pad;
    for (size_t i = 0; i < runs.size(); ++i) {
        auto const &run = runs[i];
        auto run_text = text.substr(run.start, run.length);
        shaper_.draw_run(temp_painter, run_text, run.rtl(), {x, fm.ascent + pad}, color, font_size,
                         font, bold, italic);
        x += run_widths[i];
    }
    cairo_surface_flush(surf);

    auto data = cairo_image_surface_get_data(surf);
    RasterizedText result;
    result.width = pw;
    result.height = ph;
    result.ascent = fm.ascent;
    result.x_offset = 0.0f;
    argb32_to_rgba(result.pixels, data, pw * ph);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return result;
}

Size CairoTextRasterizer::measure(std::string_view text, float font_size, FontFamily font,
                                  bool bold, bool italic) {
    if (text.empty()) {
        return {0, 0};
    }
    // Use the shaper for accurate multi-script width: single-font
    // cairo_text_extents returns advance=0 for glyphs the font lacks
    // (CJK, Hebrew, etc.), causing labels to clip those characters.
    auto base = bidi::detect_base_direction(text);
    auto bidi_line = bidi::BidiLine::analyze(text, base);
    auto width = 0.0f;
    for (auto const &run : bidi_line.runs_visual()) {
        auto run_text = text.substr(run.start, run.length);
        for (auto const &ca : shaper_.shape_run(run_text, run.rtl(), font_size, font, bold, italic)) {
            width += ca.advance;
        }
    }
    auto fm = metrics(font_size, font);
    return {width, fm.height};
}

Painter::FontMetrics CairoTextRasterizer::metrics(float font_size, FontFamily font) {
    auto surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto cr = cairo_create(surf);
    cairo_font_extents_t fe;

    cairo_select_font_face(cr, font_name_for(font).c_str(), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, std::round(font_size));
    cairo_font_extents(cr, &fe);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return {static_cast<float>(fe.ascent), static_cast<float>(fe.descent),
            static_cast<float>(fe.height)};
}

} // namespace toolkit
#endif
