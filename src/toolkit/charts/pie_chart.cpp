// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/charts/pie_chart.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace toolkit {

PieChart::PieChart() = default;

void PieChart::set_slices(std::vector<PieSlice> slices) {
    slices_ = std::move(slices);
    hover_idx_.reset();
}

void PieChart::clear() {
    slices_.clear();
    hover_idx_.reset();
}

float PieChart::total_value() const {
    auto total = 0.0f;
    for (auto const &s : slices_) {
        total += s.value;
    }
    return total;
}

void PieChart::paint(Painter &painter) {
    auto const &palette = Theme::current().palette;
    auto bg = palette.window;
    auto text_color = palette.text;
    auto font_size = palette.fonts.size > 0 ? palette.fonts.size - 1 : 12;
    auto small_font = font_size - 1;

    painter.fill_rect(rect_, bg);
    if (slices_.empty()) {
        return;
    }

    auto total = total_value();
    if (total <= 0) {
        return;
    }

    auto title_h = 0;
    if (!title_.empty()) {
        auto ts = painter.measure_text(title_, font_size + 2);
        float tx = rect_.x + (rect_.width - ts.width) / 2;
        float ty = rect_.y + 22;
        painter.draw_text(title_, {tx, ty}, text_color, font_size + 2);
        title_h = 30;
    }

    auto legend_h = show_legend_ ? 30 : 0;
    auto avail_w = rect_.width;
    auto avail_h = rect_.height - title_h - legend_h;
    auto cx = rect_.x + avail_w / 2;
    auto cy = rect_.y + title_h + avail_h / 2;
    auto radius = std::min(avail_w, avail_h) / 2 - 10;
    if (radius < 10) {
        radius = 10;
    }
    auto inner_radius = donut_ ? radius * 0.55f : 0;
    auto constexpr kSegments = 64;
    // Smallest slice sweep (radians) worth drawing; below this the slice is a
    // sliver too thin to render meaningfully.
    auto constexpr kMinSweep = 1e-5f;
    auto angle = static_cast<float>(-M_PI / 2);

    for (auto si = size_t{0}; si < slices_.size(); si++) {
        auto sweep = (slices_[si].value / total) * 2.0f * static_cast<float>(M_PI);
        if (sweep < kMinSweep) {
            continue;
        }

        auto r = radius;
        auto hovered = hover_idx_ && *hover_idx_ == si;
        if (hovered) {
            r += 6;
        }

        auto n_segs = std::max(4, static_cast<int>(kSegments * (sweep / (2 * M_PI))));
        auto seg_step = sweep / static_cast<float>(n_segs);
        if (donut_) {
            float ir = inner_radius;
            if (hovered) {
                ir += 3;
            }

            // Build polygon: outer arc forward, then inner arc backward
            std::vector<Point> poly;
            poly.reserve(2 * (n_segs + 1) + 1);
            for (auto j = 0; j <= n_segs; j++) {
                auto a = angle + seg_step * j;
                poly.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
            }
            for (int j = n_segs; j >= 0; j--) {
                float a = angle + seg_step * static_cast<float>(j);
                poly.push_back({cx + std::cos(a) * ir, cy + std::sin(a) * ir});
            }
            painter.fill_polygon(poly, slices_[si].color);
        } else {
            std::vector<Point> poly;
            poly.reserve(n_segs + 2);
            poly.push_back({cx, cy});
            for (int j = 0; j <= n_segs; j++) {
                float a = angle + seg_step * static_cast<float>(j);
                poly.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
            }
            painter.fill_polygon(poly, slices_[si].color);
        }

        // Slice border
        {
            std::vector<Point> arc;
            arc.reserve(n_segs + 1);
            for (int j = 0; j <= n_segs; j++) {
                float a = angle + seg_step * static_cast<float>(j);
                arc.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
            }
            painter.draw_polyline(arc, bg, 1.5f);
        }

        // Label at midpoint
        if (show_labels_ && sweep > 0.15f) {
            auto mid_angle = angle + sweep / 2;
            auto label_r = donut_ ? (radius + inner_radius) / 2 : radius * 0.65f;
            auto lx = cx + std::cos(mid_angle) * label_r;
            auto ly = cy + std::sin(mid_angle) * label_r;
            auto pct = slices_[si].value / total * 100;

            auto buf = fmt::format("{:.1f}%", pct);
            auto ts = painter.measure_text(buf, small_font);
            painter.draw_text(buf, {lx - ts.width / 2, ly + ts.height / 3}, Color::rgb(1, 1, 1),
                              small_font);
        }

        angle += sweep;
    }

    // Hover tooltip
    if (hover_idx_ && *hover_idx_ < slices_.size()) {
        auto const &s = slices_[*hover_idx_];
        auto pct = s.value / total * 100;

        auto tip = fmt::format("{}: {:.2f} ({:.1f}%)", s.label, s.value, pct);
        auto ts = painter.measure_text(tip, small_font);
        auto tip_fm = painter.font_metrics(small_font);
        auto tw = ts.width + 12, th = ts.height + 8;
        auto tx = cx + 10, ty = cy - radius - th - 4;
        if (tx + tw > rect_.x + rect_.width) {
            tx = rect_.x + rect_.width - tw - 4;
        }
        if (ty < rect_.y) {
            ty = rect_.y + 4;
        }
        painter.fill_rounded_rect({tx, ty, tw, th}, Color::rgba(0, 0, 0, 0.8f), 4);
        painter.draw_text(tip, {tx + 6, ty + th / 2 + (tip_fm.ascent - tip_fm.descent) / 2},
                          Color::rgb(1, 1, 1), small_font);
    }

    // Legend
    if (show_legend_) {
        auto lx = rect_.x + 12;
        auto ly = rect_.y + rect_.height - 10;
        for (auto const &s : slices_) {
            painter.fill_rect({lx, ly - 4, 10, 10}, s.color);
            lx += 14;
            painter.draw_text(s.label, {lx, ly + 4}, text_color, small_font);
            auto ns = painter.measure_text(s.label, small_font);
            lx += ns.width + 14;
        }
    }
}

bool PieChart::handle_mouse(MouseEvent const &event) {
    if (event.type != MouseEvent::Type::Move && event.type != MouseEvent::Type::Drag) {
        return false;
    }

    auto total = total_value();
    if (total <= 0 || slices_.empty()) {
        return false;
    }

    auto title_h = title_.empty() ? 0 : 30;
    auto legend_h = show_legend_ ? 30 : 0;
    auto avail_w = rect_.width;
    auto avail_h = rect_.height - title_h - legend_h;
    auto cx = rect_.x + avail_w / 2;
    auto cy = rect_.y + title_h + avail_h / 2;
    auto radius = std::min(avail_w, avail_h) / 2 - 10;
    if (radius < 10) {
        radius = 10;
    }

    auto mx = event.position.x - cx;
    auto my = event.position.y - cy;
    auto dist = std::sqrt(mx * mx + my * my);

    if (dist > radius + 10 || (donut_ && dist < radius * 0.55f - 5)) {
        if (hover_idx_) {
            hover_idx_.reset();
            if (window_) {
                window_->request_redraw();
            }
        }
        return false;
    }

    auto mouse_angle = std::atan2(my, mx);
    if (mouse_angle < static_cast<float>(-M_PI / 2)) {
        mouse_angle += 2 * static_cast<float>(M_PI);
    }
    auto offset = mouse_angle + static_cast<float>(M_PI / 2);
    if (offset < 0) {
        offset += 2 * static_cast<float>(M_PI);
    }

    auto cumulative = 0.0f;
    std::optional<size_t> found;
    for (auto i = size_t{0}; i < slices_.size(); i++) {
        auto sweep = (slices_[i].value / total) * 2 * M_PI;
        if (offset >= cumulative && offset < cumulative + sweep) {
            found = i;
            break;
        }
        cumulative += sweep;
    }

    if (found != hover_idx_) {
        hover_idx_ = found;
        if (on_hover && hover_idx_) {
            on_hover(*hover_idx_);
        }
        if (window_) {
            window_->request_redraw();
        }
    }
    return true;
}

Size PieChart::size_hint() const { return {300, 300}; }

nlohmann::json PieChart::to_json() const {
    auto j = Widget::to_json();
    j["type"] = "PieChart";
    j["title"] = title_;
    j["slices"] = slices_.size();
    return j;
}

} // namespace toolkit
