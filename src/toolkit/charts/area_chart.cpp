// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/charts/area_chart.hpp"
#include "toolkit/charts/chart_defaults.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <nlohmann/json.hpp>

namespace toolkit {

AreaChart::AreaChart() = default;

void AreaChart::add_series(AreaSeries series) { series_.push_back(std::move(series)); }

void AreaChart::clear_series() {
    series_.clear();
    hover_.reset();
    interaction_.reset_zoom();
}

AreaChart::PlotArea AreaChart::compute_plot_area() const {
    auto legend_space = (show_legend_ && !series_.empty()) ? chart_defaults::kLegendHeight : 0;
    auto title_space = title_.empty() ? 0.0f : 8.0f;
    auto y_label_space = y_label_.empty() ? 0.0f : 18.0f;

    PlotArea pa{};
    pa.x = rect_.x + chart_defaults::kMarginLeftNarrow + y_label_space;
    pa.y = rect_.y + chart_defaults::kMarginTop + title_space;
    pa.w = rect_.width - chart_defaults::kMarginLeftNarrow - chart_defaults::kMarginRight - y_label_space;
    pa.h = rect_.height - chart_defaults::kMarginTop - chart_defaults::kMarginBottom - legend_space - title_space;
    if (pa.w < 1) {
        pa.w = 1;
    }
    if (pa.h < 1) {
        pa.h = 1;
    }

    pa.data_x_min = 0;
    pa.data_x_max = 1;
    pa.data_y_min = 0;
    pa.data_y_max = 1;

    auto first = true;
    for (auto const &s : series_) {
        for (auto const &p : s.points) {
            if (first) {
                pa.data_x_min = pa.data_x_max = p.x;
                pa.data_y_min = pa.data_y_max = p.y;
                first = false;
            } else {
                pa.data_x_min = std::min(pa.data_x_min, p.x);
                pa.data_x_max = std::max(pa.data_x_max, p.x);
                pa.data_y_min = std::min(pa.data_y_min, p.y);
                pa.data_y_max = std::max(pa.data_y_max, p.y);
            }
        }
    }

    if (!first) {
        if (!auto_range_) {
            if (y_min_override_) {
                pa.data_y_min = *y_min_override_;
            }
            if (y_max_override_) {
                pa.data_y_max = *y_max_override_;
            }
        }

        auto y_range = pa.data_y_max - pa.data_y_min;
        if (y_range < chart_defaults::kMinDataRange) {
            y_range = 1.0f;
        }
        auto pad = y_range * 0.05f;
        pa.data_y_min -= pad;
        pa.data_y_max += pad;
        if (pa.data_x_max - pa.data_x_min < chart_defaults::kMinDataRange) {
            pa.data_x_max = pa.data_x_min + 1;
        }
    }
    interaction_.set_data_range(pa.data_x_min, pa.data_x_max, pa.data_y_min, pa.data_y_max);
    pa.data_x_min = interaction_.view_x_min;
    pa.data_x_max = interaction_.view_x_max;
    pa.data_y_min = interaction_.view_y_min;
    pa.data_y_max = interaction_.view_y_max;
    return pa;
}

float AreaChart::to_screen_x(PlotArea const &pa, float data_x) const {
    auto t = (data_x - pa.data_x_min) / (pa.data_x_max - pa.data_x_min);
    return pa.x + t * pa.w;
}

float AreaChart::to_screen_y(PlotArea const &pa, float data_y) const {
    auto t = (data_y - pa.data_y_min) / (pa.data_y_max - pa.data_y_min);
    return pa.y + pa.h - t * pa.h;
}

void AreaChart::compute_nice_ticks(float min_val, float max_val, int target_count,
                                   std::vector<float> &ticks_out) const {
    ticks_out.clear();
    float range = max_val - min_val;
    if (range <= 0) {
        return;
    }
    float raw_step = range / std::max(target_count, 1);
    float mag = std::pow(10.0f, std::floor(std::log10(raw_step)));
    float norm = raw_step / mag;
    float nice;
    if (norm <= 1.5f) {
        nice = 1;
    } else if (norm <= 3.5f) {
        nice = 2;
    } else if (norm <= 7.5f) {
        nice = 5;
    } else {
        nice = 10;
    }
    float step = nice * mag;
    float start = std::ceil(min_val / step) * step;
    for (float v = start; v <= max_val + step * 0.01f; v += step) {
        ticks_out.push_back(v);
    }
}

void AreaChart::paint(Painter &painter) {
    auto const &palette = Theme::current().palette;
    Color bg = palette.window;
    Color text_color = palette.text;
    Color grid_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.15f);
    Color axis_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.4f);
    float font_size = palette.fonts.size > 0 ? palette.fonts.size - 1 : 12;
    float small_font = font_size - 1;

    painter.fill_rect(rect_, bg);
    PlotArea pa = compute_plot_area();

    // Title
    if (!title_.empty()) {
        auto ts = painter.measure_text(title_, font_size + 2);
        float tx = pa.x + (pa.w - ts.width) / 2;
        float ty = rect_.y + chart_defaults::kMarginTop - 4;
        painter.draw_text(title_, {tx, ty}, text_color, font_size + 2);
    }

    // Y-axis label
    if (!y_label_.empty()) {
        float lx = rect_.x + 14;
        float ly = pa.y + pa.h / 2;
        painter.draw_text(y_label_, {lx, ly}, text_color, small_font, FontFamily::System,
                          Painter::TextOrientation::VerticalCCW);
    }

    // X-axis label
    if (!x_label_.empty()) {
        auto xs = painter.measure_text(x_label_, small_font);
        float lx = pa.x + (pa.w - xs.width) / 2;
        float ly = pa.y + pa.h + chart_defaults::kMarginBottom - 14;
        painter.draw_text(x_label_, {lx, ly}, text_color, small_font);
    }

    // Y-axis ticks and grid
    std::vector<float> y_ticks;
    compute_nice_ticks(pa.data_y_min, pa.data_y_max, 6, y_ticks);
    for (float yv : y_ticks) {
        float sy = to_screen_y(pa, yv);
        if (sy < pa.y - 1 || sy > pa.y + pa.h + 1) {
            continue;
        }
        if (show_grid_) {
            float dash_len = 4, gap_len = 4;
            float x = pa.x;
            while (x < pa.x + pa.w) {
                float end = std::min(x + dash_len, pa.x + pa.w);
                painter.draw_line({x, sy}, {end, sy}, grid_color, 1.0f);
                x = end + gap_len;
            }
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4g", static_cast<double>(yv));
        std::string label = buf + y_unit_suffix_;
        auto ts = painter.measure_text(label, small_font);
        float lx = pa.x - ts.width - 6;
        painter.draw_text(label, {lx, sy + ts.height / 3}, text_color, small_font);
    }

    // X-axis labels from first series
    if (!series_.empty() && !series_[0].points.empty()) {
        auto const &pts = series_[0].points;
        int visible = 0;
        for (auto const &p : pts) {
            float sx = to_screen_x(pa, p.x);
            if (sx >= pa.x - 1 && sx <= pa.x + pa.w + 1) {
                visible++;
            }
        }
        int n_labels = std::max(1, static_cast<int>(pa.w / 80));
        int step = std::max(1, visible / n_labels);
        int vis_idx = 0;
        for (size_t i = 0; i < pts.size(); i++) {
            float sx = to_screen_x(pa, pts[i].x);
            if (sx < pa.x - 1 || sx > pa.x + pa.w + 1) {
                continue;
            }
            if (vis_idx++ % step != 0) {
                continue;
            }
            auto const &lbl = pts[i].label;
            if (!lbl.empty()) {
                float label_font = small_font - 1;
                auto ts = painter.measure_text(lbl, label_font);
                float lx = sx - ts.width / 2;
                float ly = pa.y + pa.h + 4 + ts.height;
                painter.draw_text(lbl, {lx, ly}, text_color, label_font);
            }
        }
    }

    // Axes
    painter.draw_line({pa.x, pa.y}, {pa.x, pa.y + pa.h}, axis_color, 1.0f);
    painter.draw_line({pa.x, pa.y + pa.h}, {pa.x + pa.w, pa.y + pa.h}, axis_color, 1.0f);

    // Clip to plot area
    painter.push_clip({pa.x, pa.y, pa.w, pa.h});

    float baseline_y = to_screen_y(pa, 0);
    baseline_y = std::clamp(baseline_y, pa.y, pa.y + pa.h);

    for (auto const &s : series_) {
        if (s.points.size() < 2) {
            continue;
        }

        // Build filled polygon: line points + bottom edge
        std::vector<Point> poly;
        poly.reserve(s.points.size() + 2);
        for (auto const &p : s.points) {
            poly.push_back({to_screen_x(pa, p.x), to_screen_y(pa, p.y)});
        }
        poly.push_back({to_screen_x(pa, s.points.back().x), baseline_y});
        poly.push_back({to_screen_x(pa, s.points.front().x), baseline_y});

        painter.fill_polygon(poly, s.fill_color);

        // Draw the line on top
        for (size_t i = 0; i + 1 < s.points.size(); i++) {
            Point p0{to_screen_x(pa, s.points[i].x), to_screen_y(pa, s.points[i].y)};
            Point p1{to_screen_x(pa, s.points[i + 1].x), to_screen_y(pa, s.points[i + 1].y)};
            painter.draw_line(p0, p1, s.line_color, 1.5f);
        }
    }

    painter.pop_clip();

    // Hover
    if (hover_ && hover_->series_idx < series_.size()) {
        auto const &s = series_[hover_->series_idx];
        if (hover_->point_idx < s.points.size()) {
            float sx = hover_->screen_x;
            float sy = hover_->screen_y;

            Color cross_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.3f);
            painter.draw_line({sx, pa.y}, {sx, pa.y + pa.h}, cross_color, 1.0f);
            painter.fill_circle({sx, sy}, 4.0f, s.line_color);
            painter.draw_circle({sx, sy}, 4.0f, bg, 1.5f);

            auto const &dp = s.points[hover_->point_idx];
            char val_buf[64];
            std::snprintf(val_buf, sizeof(val_buf), "%.2f", static_cast<double>(dp.y));
            std::string tip = s.name;
            if (!dp.label.empty()) {
                tip += "  " + dp.label;
            }
            tip += std::string("  ") + val_buf + y_unit_suffix_;

            auto ts = painter.measure_text(tip, small_font);
            auto tip_fm = painter.font_metrics(small_font);
            float tw = ts.width + 12, th = ts.height + 8;
            float tx = sx + 10, ty = sy - th - 6;
            if (tx + tw > pa.x + pa.w) {
                tx = sx - tw - 10;
            }
            if (ty < pa.y) {
                ty = sy + 10;
            }

            painter.fill_rounded_rect({tx, ty, tw, th}, Color::rgba(0, 0, 0, 0.8f), 4);
            painter.draw_text(tip, {tx + 6, ty + th / 2 + (tip_fm.ascent - tip_fm.descent) / 2},
                              Color::rgb(1, 1, 1), small_font);
        }
    }

    // Legend
    if (show_legend_ && !series_.empty()) {
        float lx = pa.x;
        float ly = pa.y + pa.h + chart_defaults::kMarginBottom - 4;
        for (auto const &s : series_) {
            painter.fill_rect({lx, ly - 2, 16, 8}, s.fill_color);
            painter.draw_line({lx, ly + 2}, {lx + 16, ly + 2}, s.line_color, 2.0f);
            lx += 20;
            painter.draw_text(s.name, {lx, ly + 4}, text_color, small_font);
            auto ns = painter.measure_text(s.name, small_font);
            lx += ns.width + 16;
        }
    }

    // Zoom indicator
    if (interaction_.is_zoomed()) {
        std::string label = "Scroll to zoom, drag to pan, double-click to reset";
        auto zs = painter.measure_text(label, small_font - 1);
        float zx = pa.x + pa.w - zs.width - 4;
        float zy = pa.y + 4 + zs.height;
        painter.fill_rounded_rect({zx - 4, zy - zs.height - 2, zs.width + 8, zs.height + 6},
                                  Color::rgba(0, 0, 0, 0.5f), 3);
        painter.draw_text(label, {zx, zy - 1}, Color::rgb(1, 1, 1), small_font - 1);
    }
}

bool AreaChart::handle_mouse(MouseEvent const &event) {
    PlotArea pa = compute_plot_area();
    float mx = event.position.x;
    float my = event.position.y;
    bool in_plot = mx >= pa.x && mx <= pa.x + pa.w && my >= pa.y && my <= pa.y + pa.h;

    if (event.type == MouseEvent::Type::Scroll && in_plot) {
        if (interaction_.handle_scroll(mx, my, event.scroll_dy, event.shift, pa.x, pa.w, pa.y,
                                       pa.h)) {
            hover_.reset();
            if (window_) {
                window_->request_redraw();
            }
            return true;
        }
    }

    if (event.type == MouseEvent::Type::Press && in_plot) {
        if (event.click_count == 2) {
            interaction_.reset_zoom();
            hover_.reset();
            if (window_) {
                window_->request_redraw();
            }
            return true;
        }
        interaction_.handle_press(mx, my);
        if (window_) {
            window_->request_redraw();
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        if (interaction_.handle_release()) {
            if (window_) {
                window_->request_redraw();
            }
            return true;
        }
    }

    if (event.type == MouseEvent::Type::Drag && interaction_.panning) {
        interaction_.handle_drag(mx, my, pa.x, pa.w, pa.y, pa.h);
        hover_.reset();
        if (window_) {
            window_->request_redraw();
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        if (!in_plot) {
            if (hover_) {
                hover_.reset();
                if (window_) {
                    window_->request_redraw();
                }
            }
            return false;
        }

        auto best_dist2 = std::numeric_limits<float>::max();
        std::optional<HoverInfo> best;
        for (auto si = size_t{0}; si < series_.size(); si++) {
            for (auto pi = size_t{0}; pi < series_[si].points.size(); pi++) {
                float sx = to_screen_x(pa, series_[si].points[pi].x);
                float sy = to_screen_y(pa, series_[si].points[pi].y);
                float dx = sx - mx, dy = sy - my;
                float d2 = dx * dx + dy * dy;
                if (d2 < best_dist2) {
                    best_dist2 = d2;
                    best = HoverInfo{si, pi, sx, sy};
                }
            }
        }

        if (best_dist2 < 30 * 30) {
            hover_ = best;
        } else {
            hover_.reset();
        }

        if (window_) {
            window_->request_redraw();
        }
        return true;
    }

    return false;
}

Size AreaChart::size_hint() const { return {400, 250}; }

CursorShape AreaChart::cursor() const {
    if (interaction_.panning) {
        return CursorShape::Move;
    }
    return CursorShape::Arrow;
}

nlohmann::json AreaChart::to_json() const {
    auto j = Widget::to_json();
    j["type"] = "AreaChart";
    j["title"] = title_;
    j["series_count"] = series_.size();
    return j;
}

} // namespace toolkit
