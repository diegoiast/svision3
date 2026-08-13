// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

namespace svision3 {

struct ChartInteraction {
    float view_x_min = 0, view_x_max = 1;
    float view_y_min = 0, view_y_max = 1;

    float data_x_min = 0, data_x_max = 1;
    float data_y_min = 0, data_y_max = 1;

    bool panning = false;

    /// Set the full data range. If not currently zoomed, the view snaps to
    /// match the full range. Call this each frame before rendering.
    void set_data_range(float dx_min, float dx_max, float dy_min, float dy_max);

    /// Handle scroll-wheel zoom. Returns true if the viewport changed.
    /// mx/my are mouse screen coords; plot_* describe the plot area rect.
    /// shift_held selects Y-axis zoom instead of X-axis.
    bool handle_scroll(float mx, float my, float scroll_dy, bool shift_held, float plot_x,
                       float plot_w, float plot_y, float plot_h);

    /// Begin a pan drag. Returns true if accepted.
    bool handle_press(float mx, float my);

    /// Continue a pan drag. Returns true if the viewport changed.
    bool handle_drag(float mx, float my, float plot_x, float plot_w, float plot_y, float plot_h);

    /// End a pan drag.
    bool handle_release();

    /// Reset view to full data range.
    void reset_zoom();

    /// True when the viewport doesn't match the full data range.
    bool is_zoomed() const;

  private:
    bool zoomed_ = false;

    float pan_start_mx_ = 0, pan_start_my_ = 0;
    float pan_start_vx_min_ = 0, pan_start_vx_max_ = 0;
    float pan_start_vy_min_ = 0, pan_start_vy_max_ = 0;

    void clamp_view();
};

} // namespace svision3
