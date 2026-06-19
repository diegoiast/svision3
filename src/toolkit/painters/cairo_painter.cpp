#ifdef TOOLKIT_HAS_CAIRO
#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"
#include "toolkit/window.hpp"

#ifdef TOOLKIT_HAS_TEXT_SHAPER
#include "toolkit/painters/cairo_text_shaper.hpp"
#endif

#include <algorithm>
#include <cairo.h>
#include <spdlog/spdlog.h>
#include <string>

namespace toolkit {

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
    float rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
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

// Detect a monospace font by checking that 'i' and 'm' have equal advances.
// Uses an isolated temporary cairo context so font probing has zero side‑effect
// on any caller context.
static std::string find_monospace_font() {
    auto *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto *cr = cairo_create(surf);

    static const char *candidates[] = {
        "DejaVu Sans Mono", "Liberation Mono", "Courier New", "Noto Mono", "Hack",
        "Ubuntu Mono",      "Courier",         nullptr};
    cairo_set_font_size(cr, 12.0);
    std::string found;
    for (auto **name = candidates; *name; ++name) {
        cairo_select_font_face(cr, *name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_text_extents_t ti, tm;
        cairo_text_extents(cr, "i", &ti);
        cairo_text_extents(cr, "m", &tm);
        if (std::abs(ti.x_advance - tm.x_advance) < 0.1) {
            found = *name;
            break;
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return found.empty() ? "monospace" : found;
}

static std::string cairo_font_face(FontFamily f, cairo_t * /*cr*/) {
    if (f == FontFamily::Monospace) {
        return find_monospace_font();
    }
    auto const &name = Theme::current().palette.fonts.system;
    return name;
}

void CairoTextRasterizer::draw_text(Painter &p, std::string_view text, Point position,
                                    Color const &color, float font_size, FontFamily font,
                                    Painter::TextOrientation orientation, bool bold, bool italic) {
    if (auto *cp = dynamic_cast<CairoPainter *>(&p)) {
        auto cr = cp->cairo();
        cairo_new_path(cr);
        cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

        cairo_save(cr);
        cairo_move_to(cr, position.x, position.y);
        if (orientation == Painter::TextOrientation::VerticalCCW) {
            cairo_rotate(cr, -M_PI / 2.0);
        } else if (orientation == Painter::TextOrientation::VerticalCW) {
            cairo_rotate(cr, M_PI / 2.0);
        }

#ifdef TOOLKIT_HAS_TEXT_SHAPER
        if (orientation != Painter::TextOrientation::Horizontal) {
            std::string s{text};
            cairo_show_text(cr, s.c_str());
        } else {
            auto shaped = shaper_.shape(cr, text, font_size, font, bold, italic);
            if (!shaped.runs.empty()) {
                for (auto &run : shaped.runs) {
                    if (!run.glyphs.empty()) {
                        for (auto &g : run.glyphs) {
                            g.x += position.x;
                            g.y += position.y;
                        }
                        cairo_show_glyphs(cr, run.glyphs.data(),
                                          static_cast<int>(run.glyphs.size()));
                    }
                }
            } else {
                std::string s{text};
                cairo_show_text(cr, s.c_str());
            }
        }
#else
        cairo_select_font_face(cr, cairo_font_face(font, cr).c_str(),
                               italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
                               bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, std::round(font_size));
        std::string s{text};
        cairo_show_text(cr, s.c_str());
#endif

        auto status = cairo_status(cr);
        if (status != CAIRO_STATUS_SUCCESS) {
            spdlog::error("CairoPainter: draw_text error for '{}': {}", text,
                          cairo_status_to_string(status));
        }

        cairo_restore(cr);
    } else {
        // Fallback for non-Cairo painters: rasterize and draw as image
        float scale = p.scale_factor();
        auto rt = rasterize(text, font_size, scale, color, font, bold, italic);
        if (rt.pixels.empty()) {
            return;
        }

        Point snapped_pos = {std::floor(position.x * scale + 0.5f) / scale,
                             std::floor(position.y * scale + 0.5f) / scale};

        p.push_translation(snapped_pos);
        if (orientation == Painter::TextOrientation::VerticalCCW) {
            p.push_rotation(-90.0f);
        } else if (orientation == Painter::TextOrientation::VerticalCW) {
            p.push_rotation(90.0f);
        }

        // Add 1px padding offset from rasterize() below
        p.draw_image(ImageData{std::move(rt.pixels), rt.width, rt.height},
                     {(rt.x_offset - 1.0f) / scale, (-rt.ascent - 1.0f) / scale});

        if (orientation != Painter::TextOrientation::Horizontal) {
            p.pop_rotation();
        }
        p.pop_translation();
    }
}

static void rgba_to_cairo_argb32(uint8_t *dst, std::vector<uint8_t> const &src) {
    auto size = src.size() / 4;
    for (auto i = 0; i < size; ++i) {
        auto alpha = src[i * 4 + 3] / 255.0f;
        dst[i * 4 + 0] = static_cast<uint8_t>(src[i * 4 + 2] * alpha);
        dst[i * 4 + 1] = static_cast<uint8_t>(src[i * 4 + 1] * alpha);
        dst[i * 4 + 2] = static_cast<uint8_t>(src[i * 4 + 0] * alpha);
        dst[i * 4 + 3] = src[i * 4 + 3];
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
    rgba_to_cairo_argb32(data, image.pixels);
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
    rgba_to_cairo_argb32(data, image.pixels);
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
    float scale = window->scale_factor();
    int lw = static_cast<int>(window->size().width);
    int lh = static_cast<int>(window->size().height);
    if (lw <= 0 || lh <= 0) {
        return nullptr;
    }
    int pw = static_cast<int>(std::ceil(lw * scale));
    int ph = static_cast<int>(std::ceil(lh * scale));
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    cairo_t *cr = cairo_create(surf);
    cairo_scale(cr, scale, scale);
    CairoPainter painter(cr);
    window->handle_paint(painter);
    cairo_surface_flush(surf);

    unsigned char *data = cairo_image_surface_get_data(surf);
    auto result = std::make_shared<ImageData>();
    result->width = pw;
    result->height = ph;
    result->channels = 4;
    result->pixels.resize(pw * ph * 4);

    for (int i = 0; i < pw * ph * 4; i += 4) {
        // Cairo ARGB32 is BGRA in memory on little-endian
        result->pixels[i + 0] = data[i + 2]; // R
        result->pixels[i + 1] = data[i + 1]; // G
        result->pixels[i + 2] = data[i + 0]; // B
        result->pixels[i + 3] = data[i + 3]; // A
    }

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

    auto slant = italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL;
    auto weight = bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL;

    auto apply_font_options = [](cairo_t *cr) {
        cairo_font_options_t *fo = cairo_font_options_create();
        cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
        cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_SLIGHT);
        cairo_set_font_options(cr, fo);
        cairo_font_options_destroy(fo);
    };

    cairo_surface_t *temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *temp_cr = cairo_create(temp_surf);
    apply_font_options(temp_cr);
    cairo_select_font_face(temp_cr, cairo_font_face(font, temp_cr).c_str(), slant, weight);
    cairo_set_font_size(temp_cr, std::floor(font_size));

    cairo_text_extents_t te;
    cairo_font_extents_t fe;
    std::string str(text);
    cairo_text_extents(temp_cr, str.c_str(), &te);
    cairo_font_extents(temp_cr, &fe);
    cairo_destroy(temp_cr);
    cairo_surface_destroy(temp_surf);

    // Ink bounding box starts at x_bearing and spans width
    // Add 2px padding (1px on each side) to avoid clipping and aliasing artifacts
    auto pw = static_cast<int>(std::ceil(te.width * scale)) + 2;
    auto ph = static_cast<int>(std::ceil(fe.height * scale)) + 2;
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

    apply_font_options(cr);
    cairo_scale(cr, scale, scale);

    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
    cairo_select_font_face(cr, cairo_font_face(font, cr).c_str(), slant, weight);
    cairo_set_font_size(cr, std::floor(font_size));

    // Align ink to the left edge of our surface, plus 1px padding (logical)
    auto pad = 1.0f / scale;
    cairo_move_to(cr, -te.x_bearing + pad, fe.ascent + pad);
    cairo_show_text(cr, str.c_str());
    cairo_surface_flush(surf);

    unsigned char *data = cairo_image_surface_get_data(surf);
    RasterizedText result;
    result.width = pw;
    result.height = ph;
    result.ascent = static_cast<float>(fe.ascent);
    result.x_offset = static_cast<float>(te.x_bearing);

    auto size = static_cast<size_t>(pw * ph * 4);
    result.pixels.resize(size);
    for (auto i = 0; i < size; i += 4) {
        // FIXME: how about we do this without copying?
        // Cairo ARGB32 is BGRA in memory on little-endian
        result.pixels[i + 0] = data[i + 2]; // R
        result.pixels[i + 1] = data[i + 1]; // G
        result.pixels[i + 2] = data[i + 0]; // B
        result.pixels[i + 3] = data[i + 3]; // A
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return result;
}

Size CairoTextRasterizer::measure(std::string_view text, float font_size, FontFamily font) {
    if (text.empty()) {
        return {0, 0};
    }

    auto surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto cr = cairo_create(surf);
    cairo_font_extents_t fe;

#ifdef TOOLKIT_HAS_TEXT_SHAPER
    auto shaped = shaper_.shape(cr, text, font_size, font, false, false);
    cairo_font_extents(cr, &fe);
    cairo_destroy(cr);
    auto advance = static_cast<float>(shaped.total_advance);
#else
    cairo_select_font_face(cr, cairo_font_face(font, cr).c_str(), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, std::round(font_size));
    cairo_text_extents_t te;
    std::string s{text};
    cairo_text_extents(cr, s.c_str(), &te);
    cairo_font_extents(cr, &fe);
    advance = static_cast<float>(te.x_advance);
    cairo_destroy(cr);
#endif
    cairo_surface_destroy(surf);
    return {advance, static_cast<float>(fe.height)};
}

Painter::FontMetrics CairoTextRasterizer::metrics(float font_size, FontFamily font) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    cairo_font_extents_t fe;

    cairo_select_font_face(cr, cairo_font_face(font, cr).c_str(), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, std::round(font_size));
    cairo_font_extents(cr, &fe);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return {static_cast<float>(fe.ascent), static_cast<float>(fe.descent),
            static_cast<float>(fe.height)};
}

std::vector<double> CairoTextRasterizer::cursor_positions(std::string_view text, float font_size,
                                                          FontFamily font) {
    if (text.empty()) {
        return {0.0};
    }

#ifdef TOOLKIT_HAS_TEXT_SHAPER
    // Expand ShapedText's codepoint-indexed positions to byte-offset-indexed
    auto expand = [](std::string_view txt, std::vector<double> const &cp_pos) {
        std::vector<double> result(txt.size() + 1, 0.0);
        auto cp_idx = 0;
        auto byte_pos = 0;
        while (byte_pos < txt.size()) {
            result[byte_pos] = cp_pos[cp_idx];
            auto next = Utf8Iterator::next(txt, byte_pos);
            for (auto j = byte_pos + 1; j < next; j++) {
                result[j] = cp_pos[cp_idx];
            }
            byte_pos = next;
            cp_idx++;
        }
        result[txt.size()] = cp_pos[cp_pos.size() - 1];
        return result;
    };

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    auto shaped = shaper_.shape(cr, text, font_size, font, false, false);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    if (!shaped.cursor_positions.empty()) {
        return expand(text, shaped.cursor_positions);
    }
#endif

    // Fallback: naive LTR
    std::vector<double> pos(text.size() + 1, 0.0);
    return pos;
}

} // namespace toolkit
#endif
