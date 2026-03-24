// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image_loader.hpp"
#include "toolkit/types.hpp"
#include <string_view>

namespace toolkit {

struct WidgetStyle;

class Painter {
  public:
    virtual ~Painter() = default;

    struct FontMetrics {
        float ascent;
        float descent;
        float height;
    };

    enum class LineStyle { Solid, Dashed, Dotted };

    enum class TextOrientation {
        Horizontal,
        VerticalCCW, // 90 degrees counter-clockwise (bottom to top)
        VerticalCW   // 90 degrees clockwise (top to bottom)
    };

    virtual void push_clip(Rect const &rect) = 0;
    virtual void pop_clip() = 0;

    virtual void push_translation(Point p) = 0;
    virtual void pop_translation() = 0;

    virtual void set_line_style(LineStyle style) = 0;

    virtual void fill_rect(Rect const &rect, Color const &color) = 0;
    virtual void draw_rect(Rect const &rect, Color const &color, float line_width = 1.0f) = 0;
    virtual void fill_rounded_rect(Rect const &rect, Color const &color, float radius) = 0;
    virtual void draw_rounded_rect(Rect const &rect, Color const &color, float radius,
                                   float line_width = 1.0f) = 0;
    virtual void draw_line(Point from, Point to, Color const &color, float line_width = 1.0f) = 0;
    virtual void fill_circle(Point center, float radius, Color const &color) = 0;
    virtual void draw_circle(Point center, float radius, Color const &color,
                             float line_width = 1.0f) = 0;
    virtual void draw_text(std::string_view text, Point position, Color const &color,
                           float font_size = 14.0f, FontFamily font = FontFamily::System,
                           TextOrientation orientation = TextOrientation::Horizontal) = 0;
    virtual void draw_image(ImageData const &image, Point position) = 0;
    virtual void draw_image_scaled(ImageData const &image, Rect const &dest) = 0;

    virtual Size text_size(std::string_view text, float font_size = 14.0f,
                           FontFamily font = FontFamily::System) = 0;
    virtual FontMetrics font_metrics(float font_size, FontFamily font = FontFamily::System) = 0;

    virtual std::string_view name() const = 0;

    // Non-virtual convenience methods implemented in terms of the above
    void draw_frame(Rect const &rect, Color bg, Color border, WidgetStyle const &style,
                    bool sunken = false);
    void draw_focus_ring(Rect const &rect, float corner_radius);

    // Global text measurement -- delegates to current platform
    static Size measure_text(std::string_view text, float font_size = 14.0f,
                             FontFamily font = FontFamily::System);
    static FontMetrics measure_font_metrics(float font_size, FontFamily font = FontFamily::System);

    static float snap_to_pixel(float val, float scale);
};

// Used for testing
class MockPainter : public Painter {
  public:
    void push_clip(Rect const &) override {}
    void pop_clip() override {}
    void push_translation(Point) override {}
    void pop_translation() override {}
    void set_line_style(LineStyle) override {}
    void fill_rect(Rect const &, Color const &) override {}
    void draw_rect(Rect const &, Color const &, float) override {}
    void fill_rounded_rect(Rect const &, Color const &, float) override {}
    void draw_rounded_rect(Rect const &, Color const &, float, float) override {}
    void draw_line(Point, Point, Color const &, float) override {}
    void fill_circle(Point, float, Color const &) override {}
    void draw_circle(Point, float, Color const &, float) override {}
    void draw_text(std::string_view, Point, Color const &, float, FontFamily,
                   TextOrientation) override {}
    void draw_image(ImageData const &, Point) override {}
    void draw_image_scaled(ImageData const &, Rect const &) override {}
    Size text_size(std::string_view text, float font_size, FontFamily) override {
        return {static_cast<float>(text.size()) * font_size * 0.6f, font_size + 2.0f};
    }
    FontMetrics font_metrics(float font_size, FontFamily) override {
        return {font_size * 0.8f, font_size * 0.2f, font_size};
    }
    std::string_view name() const override { return "mock"; }
};

} // namespace toolkit
