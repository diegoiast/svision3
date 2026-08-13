// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <svision3/widget.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace svision3 {

struct PieSlice {
    std::string label;
    float value = 0;
    Color color;
};

class PieChart : public Widget {
  public:
    PieChart();

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;

    void set_slices(std::vector<PieSlice> slices);
    void clear();
    size_t slice_count() const { return slices_.size(); }

    void set_title(std::string title) { title_ = std::move(title); }
    void set_donut(bool v) { donut_ = v; }
    void set_show_legend(bool v) { show_legend_ = v; }
    void set_show_labels(bool v) { show_labels_ = v; }

    nlohmann::json to_json() const override;

    std::function<void(size_t slice_idx)> on_hover;

  private:
    std::vector<PieSlice> slices_;
    std::string title_;
    bool donut_ = false;
    bool show_legend_ = true;
    bool show_labels_ = true;
    std::optional<size_t> hover_idx_;

    float total_value() const;
};

} // namespace svision3
