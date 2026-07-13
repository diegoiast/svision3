#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <OpenGL/gl.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <GL/gl.h>
// clang-format off
#else
#include <GL/gl.h>
#endif

#include "toolkit/painters/gl_painter.hpp"
#include "toolkit/text_rasterizer.hpp"
#include <algorithm>
#include <cmath>

// ImageData::pixels is B,G,R,A (see image.hpp); GL_BGRA is core-equivalent on desktop GL since
// 1.2 (EXT_bgra) but not always in older platform headers.
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace toolkit {

static std::vector<std::pair<float, float>> rounded_rect_verts(float x, float y, float w, float h,
                                                               float rad, int seg = 8) {
    // FIXME: hardcoded values: what are these 3.0f and 2.0f?
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

GLPainter::GLPainter(float viewport_h, float scale, TextRasterizer *rasterizer)
    : Painter(rasterizer), vh_(viewport_h), scale_(scale) {}

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

void GLPainter::push_rotation(float degrees) {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glRotatef(degrees, 0, 0, 1);
}

void GLPainter::pop_rotation() {
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void GLPainter::apply_line_style() {
    switch (style_) {
    case Painter::LineStyle::Dashed:
        glDisable(GL_LINE_SMOOTH);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0x00FF);
        break;
    case Painter::LineStyle::Dotted:
        glDisable(GL_LINE_SMOOTH);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xAAAA);
        break;
    case Painter::LineStyle::Solid:
    default:
        glDisable(GL_LINE_STIPPLE);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        break;
    }
}

void GLPainter::fill_rect(Rect const &r, Color const &c) {
    glDisable(GL_LINE_STIPPLE);
    glDisable(GL_LINE_SMOOTH);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    glVertex2f(r.x, r.y);
    glVertex2f(r.x + r.width, r.y);
    glVertex2f(r.x + r.width, r.y + r.height);
    glVertex2f(r.x, r.y + r.height);
    glEnd();
}

void GLPainter::draw_rect(Rect const &r, Color const &c, float lw) {
    glColor4f(c.r, c.g, c.b, c.a);
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
    glDisable(GL_LINE_SMOOTH);
    float rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
    if (rad <= 0) {
        fill_rect(r, c);
        return;
    }
    glColor4f(c.r, c.g, c.b, c.a);
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

    glColor4f(c.r, c.g, c.b, c.a);
    glLineWidth(lw * scale_);
    apply_line_style();
    auto pts = rounded_rect_verts(r.x, r.y, r.width, r.height, rad);
    glBegin(GL_LINE_LOOP);
    for (auto &[px, py] : pts) {
        glVertex2f(px, py);
    }
    glEnd();
}

void GLPainter::fill_triangle(Point a, Point b, Point c, Color const &color) {
    glDisable(GL_LINE_STIPPLE);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLES);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glVertex2f(c.x, c.y);
    glEnd();
}

void GLPainter::draw_line(Point a, Point b, Color const &c, float lw) {
    glColor4f(c.r, c.g, c.b, c.a);
    glLineWidth(lw * scale_);
    apply_line_style();

    glBegin(GL_LINES);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glEnd();
}

void GLPainter::fill_circle(Point center, float radius, Color const &c) {
    glDisable(GL_LINE_STIPPLE);
    glColor4f(c.r, c.g, c.b, c.a);

    int seg = 64;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center.x, center.y);
    for (int i = 0; i <= seg; i++) {
        float a = 2.0f * (float)M_PI * i / seg;
        glVertex2f(center.x + radius * cosf(a), center.y + radius * sinf(a));
    }
    glEnd();

    // Draw smoothed outline to hide jagged edges
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; i++) {
        float a = 2.0f * (float)M_PI * i / seg;
        glVertex2f(center.x + radius * cosf(a), center.y + radius * sinf(a));
    }
    glEnd();
}

void GLPainter::draw_circle(Point center, float radius, Color const &c, float lw) {
    glColor4f(c.r, c.g, c.b, c.a);
    glLineWidth(lw * scale_);
    apply_line_style();
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    int seg = 64;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; i++) {
        float a = 2.0f * (float)M_PI * i / seg;
        glVertex2f(center.x + radius * cosf(a), center.y + radius * sinf(a));
    }
    glEnd();
    glDisable(GL_LINE_SMOOTH);
}

void GLPainter::draw_image(ImageData const &image, Point position) {
    if (image.width <= 0 || image.height <= 0) {
        return;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 image.pixels.data());

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 1);

    glPushMatrix();
    glTranslatef(position.x, position.y, 0);

    float w = static_cast<float>(image.width) / scale_;
    float h = static_cast<float>(image.height) / scale_;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);
    glTexCoord2f(1, 0);
    glVertex2f(w, 0);
    glTexCoord2f(1, 1);
    glVertex2f(w, h);
    glTexCoord2f(0, 1);
    glVertex2f(0, h);
    glEnd();
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
}

void GLPainter::draw_image_scaled(ImageData const &image, Rect const &dest) {
    if (image.width <= 0 || image.height <= 0 || dest.width <= 0 || dest.height <= 0) {
        return;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 image.pixels.data());

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 1);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(dest.x, dest.y);
    glTexCoord2f(1, 0);
    glVertex2f(dest.x + dest.width, dest.y);
    glTexCoord2f(1, 1);
    glVertex2f(dest.x + dest.width, dest.y + dest.height);
    glTexCoord2f(0, 1);
    glVertex2f(dest.x, dest.y + dest.height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
}

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
