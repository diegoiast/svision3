// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Declarative UI API -- chart widgets

#include "declarative.hpp"

#include "toolkit/charts/area_chart.hpp"
#include "toolkit/charts/bar_chart.hpp"
#include "toolkit/charts/candlestick_chart.hpp"
#include "toolkit/charts/histogram.hpp"
#include "toolkit/charts/line_chart.hpp"
#include "toolkit/charts/pie_chart.hpp"
#include "toolkit/charts/scatter_plot.hpp"
#include "toolkit/charts/stacked_bar_chart.hpp"

namespace ui {

inline Element<toolkit::AreaChart> area_chart() {
    return Element<toolkit::AreaChart>(std::make_unique<toolkit::AreaChart>());
}

inline Element<toolkit::BarChart> bar_chart() {
    return Element<toolkit::BarChart>(std::make_unique<toolkit::BarChart>());
}

inline Element<toolkit::CandlestickChart> candlestick_chart() {
    return Element<toolkit::CandlestickChart>(std::make_unique<toolkit::CandlestickChart>());
}

inline Element<toolkit::Histogram> histogram() {
    return Element<toolkit::Histogram>(std::make_unique<toolkit::Histogram>());
}

inline Element<toolkit::LineChart> line_chart() {
    return Element<toolkit::LineChart>(std::make_unique<toolkit::LineChart>());
}

inline Element<toolkit::PieChart> pie_chart() {
    return Element<toolkit::PieChart>(std::make_unique<toolkit::PieChart>());
}

inline Element<toolkit::ScatterPlot> scatter_plot() {
    return Element<toolkit::ScatterPlot>(std::make_unique<toolkit::ScatterPlot>());
}

inline Element<toolkit::StackedBarChart> stacked_bar_chart() {
    return Element<toolkit::StackedBarChart>(std::make_unique<toolkit::StackedBarChart>());
}

} // namespace ui
