// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/charts/chart_interaction.hpp"
#include "svision3/widget.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace svision3 {

struct CandlestickData {
    float x = 0;
    float open = 0;
    float high = 0;
    float low = 0;
    float close = 0;
    std::string label;
};

struct CandlestickSeries {
    std::string name;
    Color up_color = Color::rgb(0.30f, 0.69f, 0.29f);
    Color down_color = Color::rgb(0.91f, 0.30f, 0.24f);
    std::vector<CandlestickData> candles;
};

class CandlestickChart : public Widget {
  public:
    CandlestickChart();

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;

    void add_series(CandlestickSeries series);
    void clear_series();
    size_t series_count() const { return series_.size(); }

    void set_title(std::string title) { title_ = std::move(title); }
    void set_x_label(std::string label) { x_label_ = std::move(label); }
    void set_y_label(std::string label) { y_label_ = std::move(label); }

    void set_show_grid(bool v) { show_grid_ = v; }

    CursorShape cursor() const override;
    nlohmann::json to_json() const override;

    std::function<void(size_t series_idx, size_t candle_idx)> on_hover;

  private:
    mutable ChartInteraction interaction_;
    std::vector<CandlestickSeries> series_;
    std::string title_;
    std::string x_label_;
    std::string y_label_;
    bool show_grid_ = true;

    struct HoverInfo {
        size_t series_idx;
        size_t candle_idx;
        float screen_x;
        float screen_y;
    };
    std::optional<HoverInfo> hover_;

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
