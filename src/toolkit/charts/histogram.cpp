// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/charts/histogram.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <nlohmann/json.hpp>

namespace toolkit {

Histogram::Histogram() = default;

void Histogram::add_series(HistogramSeries series) {
    series_.push_back(std::move(series));
    hover_.reset();
}

void Histogram::clear_series() {
    series_.clear();
    hover_.reset();
    interaction_.reset_zoom();
}

std::vector<Histogram::BinnedSeries> Histogram::compute_bins() const {
    if (series_.empty()) {
        return {};
    }

    auto global_min = series_[0].values.empty() ? 0 : series_[0].values[0];
    auto global_max = global_min;
    for (auto const &s : series_) {
        for (float v : s.values) {
            global_min = std::min(global_min, v);
            global_max = std::max(global_max, v);
        }
    }

    if (global_max - global_min < 1e-9f) {
        global_min -= 0.5f;
        global_max += 0.5f;
    }

    auto n_bins = std::max(bin_count_, 1);
    auto bin_width = (global_max - global_min) / static_cast<float>(n_bins);

    std::vector<BinnedSeries> result;
    result.reserve(series_.size());
    for (auto const &s : series_) {
        BinnedSeries bs;
        bs.name = s.name;
        bs.color = s.color;
        bs.bins.resize(n_bins);

        for (int i = 0; i < n_bins; i++) {
            bs.bins[i].low = global_min + bin_width * static_cast<float>(i);
            bs.bins[i].high = bs.bins[i].low + bin_width;
            bs.bins[i].count = 0;
        }
        for (float v : s.values) {
            int idx = static_cast<int>((v - global_min) / bin_width);
            if (idx >= n_bins) {
                idx = n_bins - 1;
            }
            if (idx < 0) {
                idx = 0;
            }
            bs.bins[idx].count++;
        }

        result.push_back(std::move(bs));
    }
    return result;
}

Histogram::PlotArea Histogram::compute_plot_area(std::vector<BinnedSeries> const &binned) const {
    auto legend_space = (show_legend_ && !series_.empty()) ? kLegendHeight : 0.0f;
    auto title_space = title_.empty() ? 0.0f : 8.0f;
    auto y_label_space = y_label_.empty() ? 0.0f : 18.0f;

    PlotArea pa{};
    pa.x = rect_.x + kMarginLeft + y_label_space;
    pa.y = rect_.y + kMarginTop + title_space;
    pa.w = rect_.width - kMarginLeft - kMarginRight - y_label_space;
    pa.h = rect_.height - kMarginTop - kMarginBottom - legend_space - title_space;
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

    if (!binned.empty() && !binned[0].bins.empty()) {
        pa.data_x_min = binned[0].bins.front().low;
        pa.data_x_max = binned[0].bins.back().high;

        auto max_count = 0;
        for (auto const &bs : binned) {
            for (auto const &b : bs.bins) {
                max_count = std::max(max_count, b.count);
            }
        }

        pa.data_y_max = static_cast<float>(max_count) * 1.1f;
        if (pa.data_y_max < 1) {
            pa.data_y_max = 1;
        }
    }
    interaction_.set_data_range(pa.data_x_min, pa.data_x_max, pa.data_y_min, pa.data_y_max);
    pa.data_x_min = interaction_.view_x_min;
    pa.data_x_max = interaction_.view_x_max;
    pa.data_y_min = interaction_.view_y_min;
    pa.data_y_max = interaction_.view_y_max;
    return pa;
}

float Histogram::to_screen_x(PlotArea const &pa, float data_x) const {
    auto t = (data_x - pa.data_x_min) / (pa.data_x_max - pa.data_x_min);
    return pa.x + t * pa.w;
}

float Histogram::to_screen_y(PlotArea const &pa, float data_y) const {
    auto t = (data_y - pa.data_y_min) / (pa.data_y_max - pa.data_y_min);
    return pa.y + pa.h - t * pa.h;
}

void Histogram::compute_nice_ticks(float min_val, float max_val, int target_count,
                                   std::vector<float> &ticks_out) const {
    ticks_out.clear();
    auto range = max_val - min_val;
    if (range <= 0) {
        return;
    }
    auto raw_step = range / std::max(target_count, 1);
    auto mag = std::pow(10.0f, std::floor(std::log10(raw_step)));
    auto norm = raw_step / mag;
    auto nice = 10.0f;
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

void Histogram::paint(Painter &painter) {
    auto const &palette = Theme::current().palette;
    auto bg = palette.window;
    auto text_color = palette.text;
    auto grid_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.15f);
    auto axis_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.4f);
    auto font_size = palette.fonts.size > 0 ? palette.fonts.size - 1 : 12;
    auto small_font = font_size - 1;
    auto binned = compute_bins();
    auto pa = compute_plot_area(binned);

    painter.fill_rect(rect_, bg);

    // Title
    if (!title_.empty()) {
        auto ts = painter.measure_text(title_, font_size + 2);
        auto tx = pa.x + (pa.w - ts.width) / 2;
        auto ty = rect_.y + kMarginTop - 4;
        painter.draw_text(title_, {tx, ty}, text_color, font_size + 2);
    }

    // Y-axis label
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
        auto ly = pa.y + pa.h + kMarginBottom - 14;
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
            auto dash_len = 4, gap_len = 4;
            auto x = pa.x;
            while (x < pa.x + pa.w) {
                float end = std::min(x + dash_len, pa.x + pa.w);
                painter.draw_line({x, sy}, {end, sy}, grid_color, 1.0f);
                x = end + gap_len;
            }
        }

        // FIXME use fmt?
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4g", static_cast<double>(yv));
        auto ts = painter.measure_text(buf, small_font);
        auto lx = pa.x - ts.width - 6;
        painter.draw_text(buf, {lx, sy + ts.height / 3}, text_color, small_font);
    }

    // X-axis ticks
    std::vector<float> x_ticks;
    compute_nice_ticks(pa.data_x_min, pa.data_x_max, 8, x_ticks);
    for (auto xv : x_ticks) {
        auto sx = to_screen_x(pa, xv);
        if (sx < pa.x - 1 || sx > pa.x + pa.w + 1) {
            continue;
        }
        // FIXME use fmt?
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4g", static_cast<double>(xv));
        auto ts = painter.measure_text(buf, small_font);
        auto lx = sx - ts.width / 2;
        auto ly = pa.y + pa.h + 4 + ts.height;
        painter.draw_text(buf, {lx, ly}, text_color, small_font);
    }

    // Axes
    painter.draw_line({pa.x, pa.y}, {pa.x, pa.y + pa.h}, axis_color, 1.0f);
    painter.draw_line({pa.x, pa.y + pa.h}, {pa.x + pa.w, pa.y + pa.h}, axis_color, 1.0f);

    // Bars
    painter.push_clip({pa.x, pa.y, pa.w, pa.h});

    auto n_series = binned.size();
    auto baseline = to_screen_y(pa, 0);

    for (auto si = 0; si < n_series; si++) {
        auto const &bs = binned[si];
        auto fill = Color::rgba(bs.color.r, bs.color.g, bs.color.b, n_series > 1 ? 0.5f : 0.7f);

        for (auto bi = 0; bi < bs.bins.size(); bi++) {
            auto left = to_screen_x(pa, bs.bins[bi].low);
            auto right = to_screen_x(pa, bs.bins[bi].high);
            auto top = to_screen_y(pa, static_cast<float>(bs.bins[bi].count));
            auto h = baseline - top;
            if (h < 0.5f) {
                continue;
            }

            auto bar_w = right - left;
            if (n_series > 1) {
                auto sub_w = bar_w / static_cast<float>(n_series);
                auto x = left + sub_w * static_cast<float>(si);
                painter.fill_rect({x, top, sub_w, h}, fill);
                painter.draw_rect({x, top, sub_w, h}, bs.color, 0.5f);
            } else {
                painter.fill_rect({left, top, bar_w, h}, fill);
                painter.draw_rect({left, top, bar_w, h}, bs.color, 0.5f);
            }
        }
    }

    painter.pop_clip();

    // Hover tooltip
    if (hover_ && hover_->series_idx < binned.size()) {
        auto const &bs = binned[hover_->series_idx];
        if (hover_->bin_idx < bs.bins.size()) {
            auto const &b = bs.bins[hover_->bin_idx];
            auto sx = hover_->screen_x;
            auto sy = hover_->screen_y;

            auto cross_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.3f);
            painter.draw_line({sx, pa.y}, {sx, pa.y + pa.h}, cross_color, 1.0f);

            // FIXME use fmt
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s  [%.2f, %.2f)  count: %d", bs.name.c_str(),
                          static_cast<double>(b.low), static_cast<double>(b.high), b.count);
            std::string tip = buf;

            auto ts = painter.measure_text(tip, small_font);
            auto tip_fm = painter.font_metrics(small_font);
            auto tw = ts.width + 12, th = ts.height + 8;
            auto tx = sx + 10, ty = sy - th - 6;
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
    if (show_legend_ && binned.size() > 1) {
        auto lx = pa.x;
        auto ly = pa.y + pa.h + kMarginBottom - 4;
        for (auto const &bs : binned) {
            painter.fill_rect({lx, ly - 4, 12, 8}, bs.color);
            lx += 16;
            painter.draw_text(bs.name, {lx, ly + 4}, text_color, small_font);
            auto ns = painter.measure_text(bs.name, small_font);
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

bool Histogram::handle_mouse(MouseEvent const &event) {
    auto binned = compute_bins();
    auto pa = compute_plot_area(binned);
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
        if (!in_plot) {
            if (hover_) {
                hover_.reset();
                if (window_) {
                    window_->request_redraw();
                }
            }
            return false;
        }

        auto n_series = binned.size();
        auto baseline = to_screen_y(pa, 0);
        std::optional<HoverInfo> best;

        for (size_t bi = 0; bi < (binned.empty() ? 0 : binned[0].bins.size()); bi++) {
            auto left = to_screen_x(pa, binned[0].bins[bi].low);
            auto right = to_screen_x(pa, binned[0].bins[bi].high);
            if (mx < left || mx > right) {
                continue;
            }

            auto bar_w = right - left;
            for (auto si = 0; si < n_series; si++) {
                auto top = to_screen_y(pa, static_cast<float>(binned[si].bins[bi].count));
                auto sub_left = 0.0f;
                auto sub_right = 0.0f;
                if (n_series > 1) {
                    auto sub_w = bar_w / n_series;
                    sub_left = left + sub_w * si;
                    sub_right = sub_left + sub_w;
                } else {
                    sub_left = left;
                    sub_right = right;
                }
                if (mx >= sub_left && mx <= sub_right && my >= top && my <= baseline) {
                    best = HoverInfo{static_cast<size_t>(si), bi, (sub_left + sub_right) / 2, top};
                }
            }
            break;
        }

        hover_ = best;
        if (window_) {
            window_->request_redraw();
        }
        return true;
    }

    return false;
}

Size Histogram::size_hint() const { return {400, 250}; }

CursorShape Histogram::cursor() const {
    if (interaction_.panning) {
        return CursorShape::Move;
    }
    return CursorShape::Arrow;
}

nlohmann::json Histogram::to_json() const {
    auto j = Widget::to_json();
    j["type"] = "Histogram";
    j["title"] = title_;
    j["series_count"] = series_.size();
    return j;
}

} // namespace toolkit
