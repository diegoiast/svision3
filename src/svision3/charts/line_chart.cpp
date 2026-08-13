// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/charts/line_chart.hpp"
#include "svision3/charts/chart_defaults.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fmt/format.h>
#include <limits>
#include <nlohmann/json.hpp>

namespace svision3 {

LineChart::LineChart() = default;

void LineChart::add_series(ChartSeries series) { series_.push_back(std::move(series)); }

void LineChart::clear_series() {
    series_.clear();
    hover_.reset();
    interaction_.reset_zoom();
}

LineChart::PlotArea LineChart::compute_plot_area() const {
    auto legend_space = (show_legend_ && !series_.empty()) ? chart_defaults::kLegendHeight : 0;
    auto title_space = title_.empty() ? 0 : 8;
    auto y_label_space = y_label_.empty() ? 0 : 18;

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

    if (!series_.empty()) {
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

        if (!auto_range_) {
            if (y_min_override_) {
                pa.data_y_min = *y_min_override_;
            }
            if (y_max_override_) {
                pa.data_y_max = *y_max_override_;
            }
        }

        // 5% padding on y-axis
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

float LineChart::to_screen_x(PlotArea const &pa, float data_x) const {
    auto t = (data_x - pa.data_x_min) / (pa.data_x_max - pa.data_x_min);
    return pa.x + t * pa.w;
}

float LineChart::to_screen_y(PlotArea const &pa, float data_y) const {
    auto t = (data_y - pa.data_y_min) / (pa.data_y_max - pa.data_y_min);
    return pa.y + pa.h - t * pa.h;
}

void LineChart::compute_nice_ticks(float min_val, float max_val, int target_count,
                                   std::vector<float> &ticks_out) const {
    ticks_out.clear();
    auto range = max_val - min_val;
    if (range <= 0) {
        return;
    }

    auto raw_step = range / std::max(target_count, 1);
    auto mag = std::pow(10.0f, std::floor(std::log10(raw_step)));
    auto norm = raw_step / mag;
    auto nice = 10.f;
    if (norm <= 1.5f) {
        nice = 1;
    } else if (norm <= 3.5f) {
        nice = 2;
    } else if (norm <= 7.5f) {
        nice = 5;
    }

    auto step = nice * mag;
    auto start = std::ceil(min_val / step) * step;
    for (auto v = start; v <= max_val + step * 0.01f; v += step) {
        ticks_out.push_back(v);
    }
}

void LineChart::paint(Painter &painter) {
    auto const &palette = Theme::current().palette;
    auto bg = palette.window;
    auto text_color = palette.text;
    auto grid_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.15f);
    auto axis_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.4f);
    auto font_size = palette.fonts.size > 0 ? palette.fonts.size - 1 : 12;
    auto small_font = font_size - 1;
    auto pa = compute_plot_area();

    painter.fill_rect(rect_, bg);

    // Title
    if (!title_.empty()) {
        auto ts = painter.measure_text(title_, font_size + 2);
        auto tx = pa.x + (pa.w - ts.width) / 2;
        auto ty = rect_.y + chart_defaults::kMarginTop - 4;
        painter.draw_text(title_, {tx, ty}, text_color, font_size + 2);
    }

    // Y-axis label (rotated)
    if (!y_label_.empty()) {
        auto lx = rect_.x + 14;
        auto ly = pa.y + pa.h / 2;
        painter.draw_text(y_label_, {lx, ly}, text_color, small_font, FontFamily::System,
                          Painter::TextOrientation::VerticalCCW);
    }

    // X-axis label
    if (!x_label_.empty()) {
        auto xs = painter.measure_text(x_label_, small_font);
        auto lx = pa.x + (pa.w - xs.width) / 2;
        auto ly = pa.y + pa.h + chart_defaults::kMarginBottom - 14;
        painter.draw_text(x_label_, {lx, ly}, text_color, small_font);
    }

    // Y-axis ticks and grid
    std::vector<float> y_ticks;
    compute_nice_ticks(pa.data_y_min, pa.data_y_max, 6, y_ticks);
    for (auto yv : y_ticks) {
        auto sy = to_screen_y(pa, yv);
        if (sy < pa.y - 1 || sy > pa.y + pa.h + 1) {
            continue;
        }

        if (show_grid_) {
            // Dashed grid line
            auto dash_len = 4, gap_len = 4;
            auto x = pa.x;
            while (x < pa.x + pa.w) {
                auto end = std::min(x + dash_len, pa.x + pa.w);
                painter.draw_line({x, sy}, {end, sy}, grid_color, 1.0f);
                x = end + gap_len;
            }
        }

        // FIXE: convert to fmt::format
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4g", static_cast<double>(yv));
        auto ts = painter.measure_text(buf, small_font);
        auto lx = pa.x - ts.width - 6;
        painter.draw_text(buf, {lx, sy + ts.height / 3}, text_color, small_font);
    }

    // X-axis ticks: use labels from the first series if available
    if (!series_.empty() && !series_[0].points.empty()) {
        auto const &pts = series_[0].points;
        // Count visible points to scale label density with zoom
        auto visible = 0;
        for (auto const &p : pts) {
            auto sx = to_screen_x(pa, p.x);
            if (sx >= pa.x - 1 && sx <= pa.x + pa.w + 1) {
                visible++;
            }
        }
        auto n_labels = std::max(1, static_cast<int>(pa.w / 80));
        auto step = std::max(1, visible / n_labels);
        auto vis_idx = 0;
        for (auto i = 0; i < pts.size(); i++) {
            auto sx = to_screen_x(pa, pts[i].x);
            if (sx < pa.x - 1 || sx > pa.x + pa.w + 1) {
                continue;
            }
            if (vis_idx++ % step != 0) {
                continue;
            }

            if (show_grid_) {
                auto dash_len = 4, gap_len = 4;
                auto y = pa.y;
                while (y < pa.y + pa.h) {
                    auto end = std::min(y + dash_len, pa.y + pa.h);
                    painter.draw_line({sx, y}, {sx, end}, grid_color, 1.0f);
                    y = end + gap_len;
                }
            }

            std::string const &lbl = pts[i].label;
            if (!lbl.empty()) {
                auto label_font = small_font - 1;
                auto ts = painter.measure_text(lbl, label_font);
                auto lx = sx - ts.width / 2;
                auto ly = pa.y + pa.h + 4 + ts.height;
                painter.draw_text(lbl, {lx, ly}, text_color, label_font);
            }
        }
    }

    // Axes
    painter.draw_line({pa.x, pa.y}, {pa.x, pa.y + pa.h}, axis_color, 1.0f);
    painter.draw_line({pa.x, pa.y + pa.h}, {pa.x + pa.w, pa.y + pa.h}, axis_color, 1.0f);

    // Clip to plot area for series drawing
    painter.push_clip({pa.x, pa.y, pa.w, pa.h});

    // Draw series
    for (auto const &s : series_) {
        if (s.points.size() < 2) {
            continue;
        }
        std::vector<Point> screen_pts;
        screen_pts.reserve(s.points.size());
        for (auto const &p : s.points) {
            screen_pts.push_back({to_screen_x(pa, p.x), to_screen_y(pa, p.y)});
        }
        painter.draw_polyline(screen_pts, s.color, 1.5f);
    }

    painter.pop_clip();

    // Hover crosshair + tooltip
    if (hover_ && hover_->series_idx < series_.size()) {
        auto const &s = series_[hover_->series_idx];
        if (hover_->point_idx < s.points.size()) {
            auto sx = hover_->screen_x;
            auto sy = hover_->screen_y;

            // Vertical crosshair line
            auto cross_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.3f);
            painter.draw_line({sx, pa.y}, {sx, pa.y + pa.h}, cross_color, 1.0f);

            // Dot
            painter.fill_circle({sx, sy}, 4.0f, s.color);
            painter.draw_circle({sx, sy}, 4.0f, bg, 1.5f);

            // Tooltip box
            auto const &dp = s.points[hover_->point_idx];

            auto tip = dp.label.empty() ? fmt::format("{}  {:.2f}", s.name, dp.y)
                                        : fmt::format("{}  {}  {:.2f}", s.name, dp.label, dp.y);

            auto ts = painter.measure_text(tip, small_font);
            auto tip_fm = painter.font_metrics(small_font);
            auto tw = ts.width + 12;
            auto th = ts.height + 8;
            auto tx = sx + 10;
            auto ty = sy - th - 6;
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
        auto lx = pa.x;
        auto ly = pa.y + pa.h + chart_defaults::kMarginBottom - 4;
        for (auto const &s : series_) {
            painter.draw_line({lx, ly}, {lx + 16, ly}, s.color, 2.0f);
            lx += 20;
            painter.draw_text(s.name, {lx, ly + 4}, text_color, small_font);

            auto ns = painter.measure_text(s.name, small_font);
            lx += ns.width + 16;
        }
    }

    // Zoom indicator
    if (interaction_.is_zoomed()) {
        auto label = std::string{"Scroll to zoom, drag to pan, double-click to reset"};
        auto zs = painter.measure_text(label, small_font - 1);
        auto zx = pa.x + pa.w - zs.width - 4;
        auto zy = pa.y + 4 + zs.height;
        painter.fill_rounded_rect({zx - 4, zy - zs.height - 2, zs.width + 8, zs.height + 6},
                                  Color::rgba(0, 0, 0, 0.5f), 3);
        painter.draw_text(label, {zx, zy - 1}, Color::rgb(1, 1, 1), small_font - 1);
    }
}

bool LineChart::handle_mouse(MouseEvent const &event) {
    auto pa = compute_plot_area();
    auto mx = event.position.x;
    auto my = event.position.y;
    auto in_plot = mx >= pa.x && mx <= pa.x + pa.w && my >= pa.y && my <= pa.y + pa.h;

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
        mouse_x_ = mx;
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
        for (size_t si = 0; si < series_.size(); si++) {
            auto const &pts = series_[si].points;
            for (auto pi = size_t{0}; pi < pts.size(); pi++) {
                auto sx = to_screen_x(pa, pts[pi].x);
                auto sy = to_screen_y(pa, pts[pi].y);
                auto dx = sx - mx, dy = sy - my;
                auto d2 = dx * dx + dy * dy;
                if (d2 < best_dist2) {
                    best_dist2 = d2;
                    best = HoverInfo{si, pi, sx, sy};
                }
            }
        }

        if (best_dist2 < 30 * 30) {
            hover_ = best;
            if (on_hover && hover_) {
                on_hover(hover_->series_idx, hover_->point_idx);
            }
        } else {
            hover_.reset();
        }
        if (window_) {
            window_->request_redraw("LineChart zoom");
        }
        return true;
    }

    return false;
}

Size LineChart::size_hint() const { return {400, 250}; }

CursorShape LineChart::cursor() const {
    if (interaction_.panning) {
        return CursorShape::Move;
    }
    return CursorShape::Arrow;
}

nlohmann::json LineChart::to_json() const {
    auto j = Widget::to_json();
    j["type"] = "LineChart";
    j["title"] = title_;
    j["series_count"] = series_.size();
    return j;
}

} // namespace svision3
