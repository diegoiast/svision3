// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/charts/chart_interaction.hpp"
#include <algorithm>
#include <cmath>

namespace svision3 {

void ChartInteraction::set_data_range(float dx_min, float dx_max, float dy_min, float dy_max) {
    data_x_min = dx_min;
    data_x_max = dx_max;
    data_y_min = dy_min;
    data_y_max = dy_max;

    if (!zoomed_) {
        view_x_min = dx_min;
        view_x_max = dx_max;
        view_y_min = dy_min;
        view_y_max = dy_max;
    } else {
        clamp_view();
    }
}

bool ChartInteraction::handle_scroll(float mx, float my, float scroll_dy, bool shift_held,
                                     float plot_x, float plot_w, float plot_y, float plot_h) {
    if (plot_w < 1 || plot_h < 1) {
        return false;
    }

    auto constexpr kZoomFactor = 0.15f;
    auto factor = (scroll_dy > 0) ? (1.0f - kZoomFactor) : (1.0f + kZoomFactor);

    if (!shift_held) {
        auto t = (mx - plot_x) / plot_w;
        t = std::clamp(t, 0.0f, 1.0f);
        auto pivot = view_x_min + t * (view_x_max - view_x_min);
        auto new_half = (view_x_max - view_x_min) * factor * 0.5f;
        auto min_range = (data_x_max - data_x_min) * 0.005f;
        if (new_half * 2 < min_range) {
            return false;
        }
        view_x_min = pivot - new_half;
        view_x_max = pivot + new_half;
    } else {
        auto t = 1.0f - (my - plot_y) / plot_h;
        t = std::clamp(t, 0.0f, 1.0f);
        auto pivot = view_y_min + t * (view_y_max - view_y_min);
        auto new_half = (view_y_max - view_y_min) * factor * 0.5f;
        auto min_range = (data_y_max - data_y_min) * 0.005f;
        if (new_half * 2 < min_range) {
            return false;
        }
        view_y_min = pivot - new_half;
        view_y_max = pivot + new_half;
    }

    clamp_view();
    zoomed_ = true;
    return true;
}

bool ChartInteraction::handle_press(float mx, float my) {
    panning = true;
    pan_start_mx_ = mx;
    pan_start_my_ = my;
    pan_start_vx_min_ = view_x_min;
    pan_start_vx_max_ = view_x_max;
    pan_start_vy_min_ = view_y_min;
    pan_start_vy_max_ = view_y_max;
    return true;
}

bool ChartInteraction::handle_drag(float mx, float my, float plot_x, float plot_w, float plot_y,
                                   float plot_h) {
    if (!panning) {
        return false;
    }
    if (plot_w < 1 || plot_h < 1) {
        return false;
    }

    auto dx_pixels = mx - pan_start_mx_;
    auto dy_pixels = my - pan_start_my_;
    auto data_per_pixel_x = (pan_start_vx_max_ - pan_start_vx_min_) / plot_w;
    auto data_per_pixel_y = (pan_start_vy_max_ - pan_start_vy_min_) / plot_h;
    auto shift_x = -dx_pixels * data_per_pixel_x;
    auto shift_y = dy_pixels * data_per_pixel_y;

    view_x_min = pan_start_vx_min_ + shift_x;
    view_x_max = pan_start_vx_max_ + shift_x;
    view_y_min = pan_start_vy_min_ + shift_y;
    view_y_max = pan_start_vy_max_ + shift_y;
    clamp_view();
    zoomed_ = true;
    return true;
}

bool ChartInteraction::handle_release() {
    if (!panning) {
        return false;
    }
    panning = false;
    return true;
}

void ChartInteraction::reset_zoom() {
    view_x_min = data_x_min;
    view_x_max = data_x_max;
    view_y_min = data_y_min;
    view_y_max = data_y_max;
    zoomed_ = false;
    panning = false;
}

bool ChartInteraction::is_zoomed() const { return zoomed_; }

void ChartInteraction::clamp_view() {
    auto vw = view_x_max - view_x_min;
    auto vh = view_y_max - view_y_min;
    auto dw = data_x_max - data_x_min;
    auto dh = data_y_max - data_y_min;

    if (vw > dw) {
        view_x_min = data_x_min;
        view_x_max = data_x_max;
    } else {
        if (view_x_min < data_x_min) {
            view_x_min = data_x_min;
            view_x_max = data_x_min + vw;
        }
        if (view_x_max > data_x_max) {
            view_x_max = data_x_max;
            view_x_min = data_x_max - vw;
        }
    }

    if (vh > dh) {
        view_y_min = data_y_min;
        view_y_max = data_y_max;
    } else {
        if (view_y_min < data_y_min) {
            view_y_min = data_y_min;
            view_y_max = data_y_min + vh;
        }
        if (view_y_max > data_y_max) {
            view_y_max = data_y_max;
            view_y_min = data_y_max - vh;
        }
    }
}

} // namespace svision3
