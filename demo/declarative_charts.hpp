// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Declarative UI API -- chart widgets

#include "declarative.hpp"

#include "svision3/charts/area_chart.hpp"
#include "svision3/charts/bar_chart.hpp"
#include "svision3/charts/candlestick_chart.hpp"
#include "svision3/charts/histogram.hpp"
#include "svision3/charts/line_chart.hpp"
#include "svision3/charts/pie_chart.hpp"
#include "svision3/charts/scatter_plot.hpp"
#include "svision3/charts/stacked_bar_chart.hpp"

namespace ui {

inline Element<svision3::AreaChart> area_chart() {
    return Element<svision3::AreaChart>(std::make_unique<svision3::AreaChart>());
}

inline Element<svision3::BarChart> bar_chart() {
    return Element<svision3::BarChart>(std::make_unique<svision3::BarChart>());
}

inline Element<svision3::CandlestickChart> candlestick_chart() {
    return Element<svision3::CandlestickChart>(std::make_unique<svision3::CandlestickChart>());
}

inline Element<svision3::Histogram> histogram() {
    return Element<svision3::Histogram>(std::make_unique<svision3::Histogram>());
}

inline Element<svision3::LineChart> line_chart() {
    return Element<svision3::LineChart>(std::make_unique<svision3::LineChart>());
}

inline Element<svision3::PieChart> pie_chart() {
    return Element<svision3::PieChart>(std::make_unique<svision3::PieChart>());
}

inline Element<svision3::ScatterPlot> scatter_plot() {
    return Element<svision3::ScatterPlot>(std::make_unique<svision3::ScatterPlot>());
}

inline Element<svision3::StackedBarChart> stacked_bar_chart() {
    return Element<svision3::StackedBarChart>(std::make_unique<svision3::StackedBarChart>());
}

} // namespace ui
