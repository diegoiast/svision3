#ifdef TOOLKIT_HAS_CAIRO
#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/window.hpp"
#include <cairo.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace toolkit {

CairoPainter::CairoPainter(cairo_t *cr) : cr_(cr) {}

void CairoPainter::push_clip(Rect const &rect) {
    cairo_save(cr_);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_clip(cr_);
}

void CairoPainter::pop_clip() { cairo_restore(cr_); }

void CairoPainter::fill_rect(Rect const &rect, Color const &color) {
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    double x = std::round(rect.x);
    double y = std::round(rect.y);
    double w = std::round(rect.width);
    double h = std::round(rect.height);
    cairo_rectangle(cr_, x, y, w, h);
    cairo_fill(cr_);
}

void CairoPainter::draw_rect(Rect const &rect, Color const &color,
                              float line_width) {
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    double offset = (static_cast<int>(line_width) % 2) == 1 ? 0.5 : 0.0;
    double x = std::round(rect.x) + offset;
    double y = std::round(rect.y) + offset;
    double w = std::round(rect.width);
    double h = std::round(rect.height);
    cairo_rectangle(cr_, x, y, w, h);
    cairo_stroke(cr_);
}

static void rounded_rect_path(cairo_t *cr, Rect const &r, float radius) {
    float rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.width - rad, r.y + rad, rad, -M_PI / 2, 0);
    cairo_arc(cr, r.x + r.width - rad, r.y + r.height - rad, rad, 0,
              M_PI / 2);
    cairo_arc(cr, r.x + rad, r.y + r.height - rad, rad, M_PI / 2, M_PI);
    cairo_arc(cr, r.x + rad, r.y + rad, rad, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void CairoPainter::fill_rounded_rect(Rect const &rect, Color const &color,
                                      float radius) {
    double x = std::round(rect.x);
    double y = std::round(rect.y);
    double w = std::round(rect.width);
    double h = std::round(rect.height);
    rounded_rect_path(cr_, {static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)}, radius);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_fill(cr_);
}

void CairoPainter::draw_rounded_rect(Rect const &rect, Color const &color,
                                      float radius, float line_width) {
    double offset = (static_cast<int>(line_width) % 2) == 1 ? 0.5 : 0.0;
    double x = std::round(rect.x) + offset;
    double y = std::round(rect.y) + offset;
    double w = std::round(rect.width);
    double h = std::round(rect.height);
    rounded_rect_path(cr_, {static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)}, radius);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    cairo_stroke(cr_);
}

void CairoPainter::draw_line(Point from, Point to, Color const &color,
                             float line_width) {
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    double offset = (static_cast<int>(line_width) % 2) == 1 ? 0.5 : 0.0;
    cairo_move_to(cr_, std::round(from.x) + offset, std::round(from.y) + offset);
    cairo_line_to(cr_, std::round(to.x) + offset, std::round(to.y) + offset);
    cairo_stroke(cr_);
}
void CairoPainter::fill_circle(Point center, float radius,
                                Color const &color) {
    cairo_arc(cr_, center.x, center.y, radius, 0, 2 * M_PI);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_fill(cr_);
}

void CairoPainter::draw_circle(Point center, float radius,
                                Color const &color, float line_width) {
    cairo_arc(cr_, center.x, center.y, radius, 0, 2 * M_PI);
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_set_line_width(cr_, line_width);
    cairo_stroke(cr_);
}

static const char *cairo_font_face(FontFamily f) {
    return f == FontFamily::Monospace ? "monospace" : "sans-serif";
}

void CairoPainter::draw_text(std::string_view text, Point position,
                              Color const &color, float font_size,
                              FontFamily font) {
    cairo_set_source_rgba(cr_, color.r, color.g, color.b, color.a);
    cairo_select_font_face(cr_, cairo_font_face(font), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, font_size);
    cairo_move_to(cr_, position.x, position.y);
    std::string str(text);
    cairo_show_text(cr_, str.c_str());
}

Size CairoPainter::text_size(std::string_view text, float font_size,
                              FontFamily font) {
    cairo_select_font_face(cr_, cairo_font_face(font), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, font_size);
    cairo_text_extents_t extents;
    std::string str(text);
    cairo_text_extents(cr_, str.c_str(), &extents);
    return {static_cast<float>(extents.x_advance),
            static_cast<float>(extents.height)};
}

Painter::FontMetrics CairoPainter::font_metrics(float font_size,
                                                 FontFamily font) {
    cairo_select_font_face(cr_, cairo_font_face(font), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, font_size);
    cairo_font_extents_t fe;
    cairo_font_extents(cr_, &fe);
    return {static_cast<float>(fe.ascent), static_cast<float>(fe.descent),
            static_cast<float>(fe.ascent + fe.descent)};
}

Size cairo_measure_text(std::string_view text, float font_size,
                        FontFamily font) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    CairoPainter p(cr);
    auto sz = p.text_size(text, font_size, font);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return sz;
}

Painter::FontMetrics cairo_measure_font_metrics(float font_size,
                                                FontFamily font) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    CairoPainter p(cr);
    auto fm = p.font_metrics(font_size, font);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return fm;
}

bool cairo_save_to_png(Window *window, std::string const &path) {
    float scale = window->scale_factor();
    int lw = static_cast<int>(window->size().width);
    int lh = static_cast<int>(window->size().height);
    if (lw <= 0 || lh <= 0) return false;
    int pw = static_cast<int>(std::ceil(lw * scale));
    int ph = static_cast<int>(std::ceil(lh * scale));
    cairo_surface_t *surf =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    cairo_t *cr = cairo_create(surf);
    cairo_scale(cr, scale, scale);
    CairoPainter painter(cr);
    window->handle_paint(painter);
    cairo_surface_flush(surf);
    auto status = cairo_surface_write_to_png(surf, path.c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return status == CAIRO_STATUS_SUCCESS;
}

RasterizedText CairoTextRasterizer::rasterize(
    std::string_view text, float font_size, float scale, FontFamily font) {
    if (text.empty()) return {};

    cairo_surface_t *temp_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *temp_cr = cairo_create(temp_surf);
    cairo_select_font_face(temp_cr, cairo_font_face(font), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(temp_cr, font_size);
    
    cairo_text_extents_t te;
    cairo_font_extents_t fe;
    std::string str(text);
    cairo_text_extents(temp_cr, str.c_str(), &te);
    cairo_font_extents(temp_cr, &fe);
    cairo_destroy(temp_cr);
    cairo_surface_destroy(temp_surf);

    float lw = static_cast<float>(te.width + te.x_bearing);
    float lh = static_cast<float>(fe.height);
    
    int pw = static_cast<int>(std::ceil(lw * scale)) + 2;
    int ph = static_cast<int>(std::ceil(lh * scale)) + 2;
    if (pw <= 0 || ph <= 0) return {};

    cairo_surface_t *surf =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    cairo_t *cr = cairo_create(surf);
    cairo_scale(cr, scale, scale);

    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_select_font_face(cr, cairo_font_face(font), CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, font_size);
    
    cairo_move_to(cr, 0, fe.ascent);
    cairo_show_text(cr, str.c_str());
    cairo_surface_flush(surf);

    unsigned char *data = cairo_image_surface_get_data(surf);
    RasterizedText result;
    result.width = pw;
    result.height = ph;
    result.ascent = static_cast<float>(fe.ascent);
    
    size_t size = static_cast<size_t>(pw * ph * 4);
    result.pixels.resize(size);
    for (size_t i = 0; i < size; i += 4) {
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

Size CairoTextRasterizer::measure(std::string_view text, float font_size,
                                  FontFamily font) {
    return cairo_measure_text(text, font_size, font);
}

Painter::FontMetrics CairoTextRasterizer::metrics(float font_size,
                                                  FontFamily font) {
    return cairo_measure_font_metrics(font_size, font);
}

} // namespace toolkit
#endif
