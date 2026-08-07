// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <toolkit/charts/chart_interaction.hpp>
#include <toolkit/widget.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace toolkit {

struct HistogramSeries {
    std::string name;
    Color color;
    std::vector<float> values;
};

class Histogram : public Widget {
  public:
    Histogram();

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;

    void add_series(HistogramSeries series);
    void clear_series();
    size_t series_count() const { return series_.size(); }

    void set_title(std::string title) { title_ = std::move(title); }
    void set_x_label(std::string label) { x_label_ = std::move(label); }
    void set_y_label(std::string label) { y_label_ = std::move(label); }

    void set_bin_count(int count) { bin_count_ = count; }
    void set_show_grid(bool v) { show_grid_ = v; }
    void set_show_legend(bool v) { show_legend_ = v; }

    CursorShape cursor() const override;
    nlohmann::json to_json() const override;

    std::function<void(size_t series_idx, size_t bin_idx)> on_hover;

  private:

    mutable ChartInteraction interaction_;
    std::vector<HistogramSeries> series_;
    std::string title_;
    std::string x_label_;
    std::string y_label_;
    int bin_count_ = 20;
    bool show_grid_ = true;
    bool show_legend_ = true;

    struct Bin {
        float low, high;
        int count;
    };

    struct BinnedSeries {
        std::string name;
        Color color;
        std::vector<Bin> bins;
    };
    std::vector<BinnedSeries> compute_bins() const;

    struct HoverInfo {
        size_t series_idx;
        size_t bin_idx;
        float screen_x;
        float screen_y;
    };
    std::optional<HoverInfo> hover_;

    struct PlotArea {
        float x, y, w, h;
        float data_x_min, data_x_max;
        float data_y_min, data_y_max;
    };

    PlotArea compute_plot_area(std::vector<BinnedSeries> const &binned) const;
    float to_screen_x(PlotArea const &pa, float data_x) const;
    float to_screen_y(PlotArea const &pa, float data_y) const;
    void compute_nice_ticks(float min_val, float max_val, int target_count,
                            std::vector<float> &ticks_out) const;
};

} // namespace toolkit
