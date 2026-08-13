// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <svision3/charts/chart_interaction.hpp>
#include <svision3/widget.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace svision3 {

struct ChartDataPoint {
    float x = 0;
    float y = 0;
    std::string label;
};

struct ChartSeries {
    std::string name;
    Color color;
    std::vector<ChartDataPoint> points;
};

class LineChart : public Widget {
  public:
    LineChart();

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;

    void add_series(ChartSeries series);
    void clear_series();
    size_t series_count() const { return series_.size(); }

    void set_title(std::string title) { title_ = std::move(title); }
    std::string const &title() const { return title_; }

    void set_x_label(std::string label) { x_label_ = std::move(label); }
    void set_y_label(std::string label) { y_label_ = std::move(label); }

    void set_show_grid(bool v) { show_grid_ = v; }
    void set_show_legend(bool v) { show_legend_ = v; }
    void set_auto_range(bool v) { auto_range_ = v; }
    void set_y_range(float min, float max) {
        y_min_override_ = min;
        y_max_override_ = max;
        auto_range_ = false;
    }

    CursorShape cursor() const override;

    nlohmann::json to_json() const override;

    std::function<void(size_t series_idx, size_t point_idx)> on_hover;

  private:
    mutable ChartInteraction interaction_;
    std::vector<ChartSeries> series_;
    std::string title_;
    std::string x_label_;
    std::string y_label_;
    bool show_grid_ = true;
    bool show_legend_ = true;
    bool auto_range_ = true;
    std::optional<float> y_min_override_;
    std::optional<float> y_max_override_;

    struct HoverInfo {
        size_t series_idx;
        size_t point_idx;
        float screen_x;
        float screen_y;
    };
    std::optional<HoverInfo> hover_;
    float mouse_x_ = -1;

    struct PlotArea {
        float x, y, w, h;
        float data_x_min, data_x_max;
        float data_y_min, data_y_max;
    };

    PlotArea compute_plot_area() const;
    float to_screen_x(PlotArea const &pa, float data_x) const;
    float to_screen_y(PlotArea const &pa, float data_y) const;
    void compute_nice_ticks(float min_val, float max_val, int target_count,
                            std::vector<float> &ticks_out) const;
};

} // namespace svision3
