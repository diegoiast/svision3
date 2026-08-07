#include "toolkit/charts/bar_chart.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace toolkit {

BarChart::BarChart() = default;

void BarChart::add_series(BarSeries series) { series_.push_back(std::move(series)); }

void BarChart::clear_series() {
    series_.clear();
    hover_.reset();
    interaction_.reset_zoom();
}

BarChart::PlotArea BarChart::compute_plot_area() const {
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

    if (!series_.empty()) {
        auto first = true;
        for (auto const &s : series_) {
            for (auto const &p : s.points) {
                if (first) {
                    pa.data_x_min = pa.data_x_max = p.x;
                    pa.data_y_max = p.y;
                    first = false;
                } else {
                    pa.data_x_min = std::min(pa.data_x_min, p.x);
                    pa.data_x_max = std::max(pa.data_x_max, p.x);
                    pa.data_y_max = std::max(pa.data_y_max, p.y);
                }
            }
        }
        auto pad = pa.data_y_max * 0.05f;
        pa.data_y_min = 0;
        pa.data_y_max += pad;
        if (pa.data_x_max - pa.data_x_min < 1e-6f) {
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

float BarChart::to_screen_x(PlotArea const &pa, float data_x) const {
    auto t = (data_x - pa.data_x_min) / (pa.data_x_max - pa.data_x_min);
    return pa.x + t * pa.w;
}

float BarChart::to_screen_y(PlotArea const &pa, float data_y) const {
    auto t = (data_y - pa.data_y_min) / (pa.data_y_max - pa.data_y_min);
    return pa.y + pa.h - t * pa.h;
}

void BarChart::compute_nice_ticks(float min_val, float max_val, int target_count,
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

static std::string format_volume(float v) {
    if (v >= 1e9f) {
        return fmt::format("{:.1f}B", static_cast<double>(v / 1e9f));
    }
    if (v >= 1e6f) {
        return fmt::format("{:.1f}M", static_cast<double>(v / 1e6f));
    }
    if (v >= 1e3f) {
        return fmt::format("{:.0f}K", static_cast<double>(v / 1e3f));
    }
    return fmt::format("{:.0f}", static_cast<double>(v));
}

void BarChart::paint(Painter &painter) {
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
        float ty = rect_.y + kMarginTop - 4;
        painter.draw_text(title_, {tx, ty}, text_color, font_size + 2);
    }

    // Y-axis label (rotated)
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
        float ly = pa.y + pa.h + kMarginBottom - 14;
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
        std::string lbl = format_volume(yv);
        auto ts = painter.measure_text(lbl, small_font);
        float lx = pa.x - ts.width - 6;
        painter.draw_text(lbl, {lx, sy + ts.height / 3}, text_color, small_font);
    }

    // X-axis date labels from first series
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
            if (!pts[i].label.empty()) {
                float label_font = small_font - 1;
                auto ts = painter.measure_text(pts[i].label, label_font);
                float lx = sx - ts.width / 2;
                float ly = pa.y + pa.h + 4 + ts.height;
                painter.draw_text(pts[i].label, {lx, ly}, text_color, label_font);
            }
        }
    }

    // Axes
    painter.draw_line({pa.x, pa.y}, {pa.x, pa.y + pa.h}, axis_color, 1.0f);
    painter.draw_line({pa.x, pa.y + pa.h}, {pa.x + pa.w, pa.y + pa.h}, axis_color, 1.0f);

    // Bars -- side-by-side when multiple series
    painter.push_clip({pa.x, pa.y, pa.w, pa.h});
    {
        size_t max_pts = 0;
        for (auto const &s : series_) {
            max_pts = std::max(max_pts, s.points.size());
        }
        int n_series = static_cast<int>(series_.size());
        float slot_w = (max_pts > 0) ? pa.w / static_cast<float>(max_pts) * 0.7f : 10.0f;
        slot_w = std::max(slot_w, 1.0f);
        float bar_w = (n_series > 1) ? slot_w / static_cast<float>(n_series) : slot_w;
        bar_w = std::max(bar_w, 1.0f);
        float baseline = to_screen_y(pa, 0);

        for (int si = 0; si < n_series; si++) {
            auto const &s = series_[si];
            float offset =
                (n_series > 1) ? -slot_w / 2 + bar_w * static_cast<float>(si) + bar_w / 2 : 0;
            for (auto const &p : s.points) {
                float cx = to_screen_x(pa, p.x) + offset;
                float top = to_screen_y(pa, p.y);
                float h = baseline - top;
                if (h < 0.5f) {
                    continue;
                }
                Color c = (p.color.a > 0) ? p.color : s.default_color;
                painter.fill_rect({cx - bar_w / 2, top, bar_w, h}, c);
            }
        }
    }
    painter.pop_clip();

    // Hover tooltip
    if (hover_ && !series_.empty() && hover_->series_idx < series_.size()) {
        auto const &s = series_[hover_->series_idx];
        if (hover_->bar_idx < s.points.size()) {
            auto const &bp = s.points[hover_->bar_idx];
            float sx = hover_->screen_x;
            float sy = hover_->screen_y;

            Color cross_color = Color::rgba(text_color.r, text_color.g, text_color.b, 0.3f);
            painter.draw_line({sx, pa.y}, {sx, pa.y + pa.h}, cross_color, 1.0f);

            std::string tip = s.name;
            if (!bp.label.empty()) {
                tip += "  " + bp.label;
            }
            tip += "  " + format_volume(bp.y);
            if (!bp.detail.empty()) {
                tip += "  " + bp.detail;
            }

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
        float ly = pa.y + pa.h + kMarginBottom - 4;
        for (auto const &s : series_) {
            painter.fill_rect({lx, ly - 4, 12, 8}, s.default_color);
            lx += 16;
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

bool BarChart::handle_mouse(MouseEvent const &event) {
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

        size_t max_pts = 0;
        for (auto const &s : series_) {
            max_pts = std::max(max_pts, s.points.size());
        }
        int n_series = static_cast<int>(series_.size());
        float slot_w = (max_pts > 0) ? pa.w / static_cast<float>(max_pts) * 0.7f : 10.0f;
        slot_w = std::max(slot_w, 1.0f);
        float bar_w = (n_series > 1) ? slot_w / static_cast<float>(n_series) : slot_w;
        bar_w = std::max(bar_w, 1.0f);
        float baseline = to_screen_y(pa, 0);

        std::optional<HoverInfo> best;
        for (int si = 0; si < n_series; si++) {
            float offset =
                (n_series > 1) ? -slot_w / 2 + bar_w * static_cast<float>(si) + bar_w / 2 : 0;
            auto const &pts = series_[si].points;
            for (size_t bi = 0; bi < pts.size(); bi++) {
                float cx = to_screen_x(pa, pts[bi].x) + offset;
                float top = to_screen_y(pa, pts[bi].y);
                float bar_left = cx - bar_w / 2;
                float bar_right = cx + bar_w / 2;
                if (mx >= bar_left && mx <= bar_right && my >= top && my <= baseline) {
                    best = HoverInfo{static_cast<size_t>(si), bi, cx, top};
                }
            }
        }

        hover_ = best;

        if (window_) {
            window_->request_redraw();
        }
        return true;
    }

    return false;
}

Size BarChart::size_hint() const { return {400, 250}; }

CursorShape BarChart::cursor() const {
    if (interaction_.panning) {
        return CursorShape::Move;
    }
    return CursorShape::Arrow;
}

nlohmann::json BarChart::to_json() const {
    auto j = Widget::to_json();
    j["type"] = "BarChart";
    j["title"] = title_;
    return j;
}

} // namespace toolkit
