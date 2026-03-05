#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <OpenGL/gl.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GL/gl.h>
#include <windows.h>
#else
#include <GL/gl.h>
#endif

#include "toolkit/painters/gl_painter.hpp"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace toolkit {

static std::vector<std::pair<float, float>> rounded_rect_verts(float x, float y, float w, float h,
                                                               float rad, int seg = 8) {
    std::vector<std::pair<float, float>> pts;
    for (int i = 0; i <= seg; i++) {
        float a = (float)M_PI + (float)M_PI / 2.0f * i / seg;
        pts.push_back({x + rad + rad * cosf(a), y + rad + rad * sinf(a)});
    }
    for (int i = 0; i <= seg; i++) {
        float a = 3.0f * (float)M_PI / 2.0f + (float)M_PI / 2.0f * i / seg;
        pts.push_back({x + w - rad + rad * cosf(a), y + rad + rad * sinf(a)});
    }
    for (int i = 0; i <= seg; i++) {
        float a = (float)M_PI / 2.0f * i / seg;
        pts.push_back({x + w - rad + rad * cosf(a), y + h - rad + rad * sinf(a)});
    }
    for (int i = 0; i <= seg; i++) {
        float a = (float)M_PI / 2.0f + (float)M_PI / 2.0f * i / seg;
        pts.push_back({x + rad + rad * cosf(a), y + h - rad + rad * sinf(a)});
    }
    return pts;
}

GLPainter::GLPainter(float viewport_h, float scale, TextRasterizer &rasterizer)
    : vh_(viewport_h), scale_(scale), rasterizer_(rasterizer) {}

void GLPainter::push_clip(Rect const &r) {
    Rect eff = r;
    if (!translations_.empty()) {
        auto const &t = translations_.back();
        eff.x += t.x;
        eff.y += t.y;
    }
    if (!clips_.empty()) {
        auto const &top = clips_.back();
        auto x0 = std::max(eff.x, top.x);
        auto y0 = std::max(eff.y, top.y);
        auto x1 = std::min(eff.x + eff.width, top.x + top.width);
        auto y1 = std::min(eff.y + eff.height, top.y + top.height);
        eff = {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
    }
    clips_.push_back(eff);
    apply_scissor(eff);
}

void GLPainter::pop_clip() {
    clips_.pop_back();
    if (clips_.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        apply_scissor(clips_.back());
    }
}

void GLPainter::set_line_style(Painter::LineStyle style) { style_ = style; }

void GLPainter::push_translation(Point p) {
    if (translations_.empty()) {
        translations_.push_back(p);
    } else {
        auto last = translations_.back();
        translations_.push_back({last.x + p.x, last.y + p.y});
    }
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(p.x, p.y, 0);
}

void GLPainter::pop_translation() {
    translations_.pop_back();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void GLPainter::apply_line_style() {
    switch (style_) {
    case Painter::LineStyle::Dashed:
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xF0F0);
        break;
    case Painter::LineStyle::Dotted:
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xAAAA);
        break;
    case Painter::LineStyle::Solid:
    default:
        glDisable(GL_LINE_STIPPLE);
        break;
    }
}

void GLPainter::fill_rect(Rect const &r, Color const &c) {
    glDisable(GL_LINE_STIPPLE);
    set_color(c);
    glBegin(GL_QUADS);
    glVertex2f(r.x, r.y);
    glVertex2f(r.x + r.width, r.y);
    glVertex2f(r.x + r.width, r.y + r.height);
    glVertex2f(r.x, r.y + r.height);
    glEnd();
}

void GLPainter::draw_rect(Rect const &r, Color const &c, float lw) {
    set_color(c);
    glLineWidth(lw * scale_);
    apply_line_style();

    glBegin(GL_LINE_LOOP);
    glVertex2f(r.x, r.y);
    glVertex2f(r.x + r.width, r.y);
    glVertex2f(r.x + r.width, r.y + r.height);
    glVertex2f(r.x, r.y + r.height);
    glEnd();
}

void GLPainter::fill_rounded_rect(Rect const &r, Color const &c, float radius) {
    glDisable(GL_LINE_STIPPLE);
    float rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
    if (rad <= 0) {
        fill_rect(r, c);
        return;
    }
    set_color(c);
    auto pts = rounded_rect_verts(r.x, r.y, r.width, r.height, rad);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(r.x + r.width / 2, r.y + r.height / 2);
    for (auto &[px, py] : pts) {
        glVertex2f(px, py);
    }
    glVertex2f(pts[0].first, pts[0].second);
    glEnd();
}

void GLPainter::draw_rounded_rect(Rect const &r, Color const &c, float radius, float lw) {
    float rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
    if (rad <= 0) {
        draw_rect(r, c, lw);
        return;
    }

    set_color(c);
    glLineWidth(lw * scale_);
    apply_line_style();
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    auto pts = rounded_rect_verts(r.x, r.y, r.width, r.height, rad);
    glBegin(GL_LINE_LOOP);
    for (auto &[px, py] : pts) {
        glVertex2f(px, py);
    }
    glEnd();
    glDisable(GL_LINE_SMOOTH);
}

void GLPainter::draw_line(Point a, Point b, Color const &c, float lw) {
    set_color(c);
    glLineWidth(lw * scale_);
    apply_line_style();
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glBegin(GL_LINES);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glEnd();
    glDisable(GL_LINE_SMOOTH);
}

void GLPainter::fill_circle(Point center, float radius, Color const &c) {
    glDisable(GL_LINE_STIPPLE);
    set_color(c);
    glEnable(GL_POLYGON_SMOOTH);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    int seg = 24;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center.x, center.y);
    for (int i = 0; i <= seg; i++) {
        float a = 2.0f * (float)M_PI * i / seg;
        glVertex2f(center.x + radius * cosf(a), center.y + radius * sinf(a));
    }
    glEnd();
    glDisable(GL_POLYGON_SMOOTH);
}

void GLPainter::draw_circle(Point center, float radius, Color const &c, float lw) {
    set_color(c);
    glLineWidth(lw * scale_);
    apply_line_style();
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    int seg = 24;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; i++) {
        float a = 2.0f * (float)M_PI * i / seg;
        glVertex2f(center.x + radius * cosf(a), center.y + radius * sinf(a));
    }
    glEnd();
    glDisable(GL_LINE_SMOOTH);
}

void GLPainter::draw_text(std::string_view text, Point pos, Color const &c, float font_size,
                          FontFamily font, TextOrientation orientation) {
    auto rt = rasterizer_.rasterize(text, font_size, scale_, font);
    if (rt.width <= 0 || rt.height <= 0) {
        return;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rt.width, rt.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rt.pixels.data());

    glEnable(GL_TEXTURE_2D);
    // Note: GL_BLEND is already enabled globally by the platform code.
    // Cairo provides premultiplied alpha, so we use GL_ONE here.
    glColor4f(c.r * c.a, c.g * c.a, c.b * c.a, c.a);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(pos.x, pos.y, 0);
    if (orientation == TextOrientation::VerticalCCW) {
        glRotatef(-90.0f, 0, 0, 1);
    } else if (orientation == TextOrientation::VerticalCW) {
        glRotatef(90.0f, 0, 0, 1);
    }

    float top_y = -rt.ascent;
    float left_x = 0;
    float qw = static_cast<float>(rt.width) / scale_;
    float qh = static_cast<float>(rt.height) / scale_;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(left_x, top_y);
    glTexCoord2f(1, 0);
    glVertex2f(left_x + qw, top_y);
    glTexCoord2f(1, 1);
    glVertex2f(left_x + qw, top_y + qh);
    glTexCoord2f(0, 1);
    glVertex2f(left_x, top_y + qh);
    glEnd();
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDeleteTextures(1, &tex);
}

Size GLPainter::text_size(std::string_view text, float font_size, FontFamily font) {
    return rasterizer_.measure(text, font_size, font);
}

GLPainter::FontMetrics GLPainter::font_metrics(float font_size, FontFamily font) {
    return rasterizer_.metrics(font_size, font);
}

void GLPainter::set_color(Color const &c) { glColor4f(c.r, c.g, c.b, c.a); }

void GLPainter::apply_scissor(Rect const &r) {
    glEnable(GL_SCISSOR_TEST);
    auto ix = static_cast<int>(std::floor(r.x * scale_));
    auto iy = static_cast<int>(std::floor((vh_ - (r.y + r.height)) * scale_));
    auto iw = static_cast<int>(std::ceil((r.x + r.width) * scale_)) - ix;
    auto ih = static_cast<int>(std::ceil((vh_ - r.y) * scale_)) - iy;

    glScissor(ix, iy, iw, ih);
}

} // namespace toolkit

#if defined(__APPLE__)
#pragma clang diagnostic pop
#endif
