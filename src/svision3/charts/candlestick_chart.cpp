// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/charts/candlestick_chart.hpp"
#include "svision3/charts/chart_defaults.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"

#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include <limits>
#include <nlohmann/json.hpp>

namespace svision3 {

CandlestickChart::CandlestickChart() = default;

void CandlestickChart::add_series(CandlestickSeries series) {
    series_.push_back(std::move(series));
    hover_.reset();
}

void CandlestickChart::clear_series() {
    series_.clear();
    hover_.reset();
    interaction_.reset_zoom();
}

CandlestickChart::PlotArea CandlestickChart::compute_plot_area() const {
    auto title_space = title_.empty() ? 0.0f : 8.0f;
    auto y_label_space = y_label_.empty() ? 0.0f : 18.0f;

    PlotArea pa{};
    pa.x = rect_.x + chart_defaults::kMarginLeftWide + y_label_space;
    pa.y = rect_.y + chart_defaults::kMarginTop + title_space;
    pa.w = rect_.width - chart_defaults::kMarginLeftWide - chart_defaults::kMarginRight - y_label_space;
    pa.h = rect_.height - chart_defaults::kMarginTop - chart_defaults::kMarginBottom - title_space;
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
        for (auto const &c : s.candles) {
            if (first) {
                pa.data_x_min = pa.data_x_max = c.x;
                pa.data_y_min = c.low;
                pa.data_y_max = c.high;
                first = false;
            } else {
                pa.data_x_min = std::min(pa.data_x_min, c.x);
                pa.data_x_max = std::max(pa.data_x_max, c.x);
                pa.data_y_min = std::min(pa.data_y_min, c.low);
                pa.data_y_max = std::max(pa.data_y_max, c.high);
            }
        }
    }

    if (!first) {
        float y_range = pa.data_y_max - pa.data_y_min;
        if (y_range < chart_defaults::kMinDataRange) {
            y_range = 1.0f;
        }
        float pad = y_range * 0.05f;
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

float CandlestickChart::to_screen_x(PlotArea const &pa, float data_x) const {
    auto t = (data_x - pa.data_x_min) / (pa.data_x_max - pa.data_x_min);
    return pa.x + t * pa.w;
}

float CandlestickChart::to_screen_y(PlotArea const &pa, float data_y) const {
    auto t = (data_y - pa.data_y_min) / (pa.data_y_max - pa.data_y_min);
    return pa.y + pa.h - t * pa.h;
}

void CandlestickChart::compute_nice_ticks(float min_val, float max_val, int target_count,
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

void CandlestickChart::paint(Painter &painter) {
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
            auto dash_len = 4, gap_len = 4;
            auto x = pa.x;
            while (x < pa.x + pa.w) {
                auto end = std::min(x + dash_len, pa.x + pa.w);
                painter.draw_line({x, sy}, {end, sy}, grid_color, 1.0f);
                x = end + gap_len;
            }
        }

        auto buf = fmt::format("{:.4g}", yv);
        auto ts = painter.measure_text(buf, small_font);
        float lx = pa.x - ts.width - 6;
        painter.draw_text(buf, {lx, sy + ts.height / 3}, text_color, small_font);
    }

    // X-axis date labels from first series
    if (!series_.empty() && !series_[0].candles.empty()) {
        auto const &candles = series_[0].candles;
        auto visible = 0;
        for (auto const &c : candles) {
            auto sx = to_screen_x(pa, c.x);
            if (sx >= pa.x - 1 && sx <= pa.x + pa.w + 1) {
                visible++;
            }
        }
        auto n_labels = std::max(1, static_cast<int>(pa.w / 80));
        auto step = std::max(1, visible / n_labels);
        auto vis_idx = 0;
        for (auto i = 0; i < candles.size(); i++) {
            auto sx = to_screen_x(pa, candles[i].x);
            if (sx < pa.x - 1 || sx > pa.x + pa.w + 1) {
                continue;
            }
            if (vis_idx++ % step != 0) {
                continue;
            }
            auto const &lbl = candles[i].label;
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

    // Candlesticks -- multiple series drawn side by side
    painter.push_clip({pa.x, pa.y, pa.w, pa.h});

    auto max_candles = size_t{0};
    for (auto const &s : series_) {
        max_candles = std::max(max_candles, s.candles.size());
    }

    if (max_candles > 0) {
        auto n_series = static_cast<int>(series_.size());
        auto slot_w = pa.w / static_cast<float>(max_candles);
        auto candle_w = (slot_w * 0.7f) / static_cast<float>(n_series);
        candle_w = std::clamp(candle_w, 1.0f, 10.0f);
        auto group_w = candle_w * static_cast<float>(n_series);

        for (auto si = 0; si < n_series; si++) {
            auto const &s = series_[si];
            auto offset = -group_w / 2.0f + candle_w * static_cast<float>(si) + candle_w / 2.0f;

            for (auto const &c : s.candles) {
                auto up = c.close >= c.open;
                auto color = up ? s.up_color : s.down_color;
                auto cx = to_screen_x(pa, c.x) + offset;
                auto wick_top = to_screen_y(pa, c.high);
                auto wick_bot = to_screen_y(pa, c.low);
                painter.draw_line({cx, wick_top}, {cx, wick_bot}, color, 1.0f);

                auto body_top = to_screen_y(pa, std::max(c.open, c.close));
                auto body_bot = to_screen_y(pa, std::min(c.open, c.close));
                auto body_h = std::max(body_bot - body_top, 1.0f);
                painter.fill_rect({cx - candle_w / 2, body_top, candle_w, body_h}, color);
            }
        }
    }
    painter.pop_clip();

    // Hover tooltip
    if (hover_ && hover_->series_idx < series_.size()) {
        auto const &s = series_[hover_->series_idx];
        if (hover_->candle_idx < s.candles.size()) {
            auto const &c = s.candles[hover_->candle_idx];
            auto sx = hover_->screen_x;
            auto sy = hover_->screen_y;
            auto cross_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.3f);
            painter.draw_line({sx, pa.y}, {sx, pa.y + pa.h}, cross_color, 1.0f);

            auto tip = fmt::format("{} {}  O:{:.2f} H:{:.2f} L:{:.2f} C:{:.2f}", s.name, c.label,
                                   c.open, c.high, c.low, c.close);

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
    if (series_.size() > 1) {
        auto lx = pa.x;
        auto ly = pa.y + pa.h + chart_defaults::kMarginBottom - 4;
        for (auto const &s : series_) {
            painter.fill_rect({lx, ly - 4, 12, 8}, s.up_color);
            lx += 16;
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

bool CandlestickChart::handle_mouse(MouseEvent const &event) {
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
        for (auto si = 0u; si < series_.size(); si++) {
            for (auto ci = 0u; ci < series_[si].candles.size(); ci++) {
                auto sx = to_screen_x(pa, series_[si].candles[ci].x);
                auto sy = to_screen_y(pa, series_[si].candles[ci].close);
                auto dx = sx - mx, dy = sy - my;
                auto d2 = dx * dx + dy * dy;
                if (d2 < best_dist2) {
                    best_dist2 = d2;
                    auto high_sy = to_screen_y(pa, series_[si].candles[ci].high);
                    best = HoverInfo{si, ci, sx, high_sy};
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

Size CandlestickChart::size_hint() const { return {400, 250}; }

CursorShape CandlestickChart::cursor() const {
    if (interaction_.panning) {
        return CursorShape::Move;
    }
    return CursorShape::Arrow;
}

nlohmann::json CandlestickChart::to_json() const {
    auto j = Widget::to_json();
    j["type"] = "CandlestickChart";
    j["title"] = title_;
    j["series_count"] = series_.size();
    return j;
}

} // namespace svision3
