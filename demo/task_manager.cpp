// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Emulates the "Performance"/"Processes" tabs of the Windows 11 Task Manager:
// a vertical sidebar (CPU, Memory, Network, Processes). CPU/Memory each plot
// a sliding 10-minute window sampled once a second; Processes lists running
// processes refreshed every 2 seconds.
//
// CPU/Memory are backed by hwinfo. hwinfo has no live network monitoring or
// process enumeration at all, so both the Network and Processes tabs go
// through process_info.hpp instead -- see that header for how a Windows/macOS
// implementation slots in without touching this file.

#include "declarative_charts.hpp"
#include "process_info.hpp"

#include "toolkit/application.hpp"
#include "toolkit/clipboard.hpp"
#include "toolkit/context_menu.hpp"
#include "toolkit/item_model.hpp"
#include "toolkit/menu.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/tray_icon.hpp"

#include <hwinfo/monitoring/cpu.h>
#include <hwinfo/monitoring/ram.h>
#include <hwinfo/ram.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>

namespace {

constexpr auto kSampleIntervalSec = 1.0f;
constexpr auto kWindowSeconds = size_t{600}; // 10 minutes at 1 sample/sec
constexpr auto kProcessRefreshIntervalSec = 2.0f;

// A single sliding-window series bound to one AreaChart. append() drops the
// oldest sample once the window is full, rebuilds the chart's series from the
// remaining points, and requests a redraw -- everything a new sample needs.
struct SlidingChart {
    toolkit::AreaChart *chart;
    // Weak, not a raw Window*: these SlidingCharts are function-local statics
    // driven by a timer and a background monitor thread, so they outlive the
    // window at shutdown. lock() turns that into a no-op instead of a dangling
    // deref.
    std::weak_ptr<toolkit::Window> window;
    std::string series_name;
    toolkit::Color line_color;
    toolkit::Color fill_color;
    std::deque<toolkit::AreaDataPoint> points;
    float next_x = 0;

    void append(float y) {
        points.push_back({next_x, y, ""});
        next_x += kSampleIntervalSec;
        while (points.size() > kWindowSeconds) {
            points.pop_front();
        }

        auto series = toolkit::AreaSeries{};
        series.name = series_name;
        series.line_color = line_color;
        series.fill_color = fill_color;
        series.points.assign(points.begin(), points.end());
        chart->clear_series();
        chart->add_series(std::move(series));
        if (auto win = window.lock()) {
            win->request_redraw();
        }
    }
};

// One tiny scrolling per-core graph, styled after Windows Task Manager's
// "Logical processors" grid. AreaChart/LineChart's fixed chart_defaults
// margins (title/axis/legend space) don't leave any room to work with at
// this size, so this paints directly with Painter instead of wrapping one.
class CoreSparkline : public toolkit::Widget, public toolkit::Fluent<CoreSparkline> {
    DECLARE_WIDGET(CoreSparkline)
  public:
    explicit CoreSparkline(int core_index) : core_index_(core_index) {}

    void append(float percent) {
        samples_.push_back(std::clamp(percent, 0.0f, 100.0f));
        while (samples_.size() > kMaxSamples) {
            samples_.pop_front();
        }
        if (window()) {
            window()->request_redraw();
        }
    }

    void paint(toolkit::Painter &painter) override {
        auto const &palette = toolkit::Theme::current().palette;
        // Widget::draw() already pushes a translation for rect_'s position
        // before calling paint(), so drawing happens in *local* coordinates
        // (origin at the widget's own top-left) -- reusing rect_.x/y here
        // would double-apply the offset for every card but the first.
        auto r = toolkit::Rect{0, 0, rect_.width, rect_.height};

        painter.fill_rounded_rect(r, palette.base, 6.0f);
        painter.draw_rounded_rect(r, palette.border, 6.0f, 1.0f);

        auto const label_size = 15.0f;
        auto label = fmt::format("CPU {}", core_index_);
        auto fm = painter.font_metrics(label_size);
        painter.draw_text(label, {r.x + 10, r.y + 8 + fm.ascent}, palette.text, label_size);

        auto pct_text = samples_.empty() ? std::string("--") : fmt::format("{:.0f}%", samples_.back());
        auto pct_w = painter.measure_text(pct_text, label_size).width;
        painter.draw_text(pct_text, {r.x + r.width - 10 - pct_w, r.y + 8 + fm.ascent},
                          palette.text_disabled, label_size);

        auto plot = toolkit::Rect{r.x + 8, r.y + fm.height + 16, r.width - 16,
                                  r.height - fm.height - 26};
        if (samples_.size() < 2 || plot.width <= 0 || plot.height <= 0) {
            return;
        }

        auto n = samples_.size();
        auto points = std::vector<toolkit::Point>{};
        points.reserve(n);
        for (size_t i = 0; i < n; i++) {
            auto x = plot.x + plot.width * static_cast<float>(i) / static_cast<float>(n - 1);
            auto y = plot.y + plot.height * (1.0f - samples_[i] / 100.0f);
            points.push_back({x, y});
        }

        auto fill_points = points;
        fill_points.push_back({points.back().x, plot.y + plot.height});
        fill_points.push_back({points.front().x, plot.y + plot.height});
        painter.fill_polygon(fill_points, fill_color_);
        painter.draw_polyline(points, line_color_, 2.0f);
    }

    bool handle_mouse(toolkit::MouseEvent const &) override { return false; }
    // A floor, not a target -- add_widget()'s stretch=1 (see main()) grows
    // these to fill the tab, so this only matters as the minimum before that
    // kicks in and as the size when there are few enough cores that stretch
    // has little to work with.
    toolkit::Size size_hint() const override { return {200, 140}; }

  private:
    static constexpr size_t kMaxSamples = 60; // 1 minute at 1 sample/sec

    int core_index_;
    std::deque<float> samples_;
    toolkit::Color line_color_ = toolkit::Color::rgb(0.20f, 0.55f, 0.90f);
    toolkit::Color fill_color_ = toolkit::Color::rgba(0.20f, 0.55f, 0.90f, 0.25f);
};

} // namespace

int main(int argc, char *argv[]) {
    auto screenshot_path = std::string{};
    for (auto i = 1; i < argc; i++) {
        auto arg = std::string{argv[i]};
        if (arg.starts_with("--screenshot=")) {
            screenshot_path = arg.substr(13);
        }
    }

    toolkit::Theme::set_current(
        toolkit::ThemeFactory::create(toolkit::ThemeStyle::MacOS, toolkit::ColorScheme::Light));

    auto app = toolkit::Application{};
    auto window = app.create_window("Task Manager", {820, 520});

    auto cpu_chart = ui::area_chart();
    cpu_chart->set_title("CPU Usage");
    cpu_chart->set_y_unit_suffix("%");
    cpu_chart->set_y_range(0.0f, 100.0f);
    cpu_chart->set_show_legend(false);
    auto cpu_chart_ptr = cpu_chart.get();

    auto total_bytes = hwinfo::Memory().size();
    auto total_gb = static_cast<float>(total_bytes) / (1024.0f * 1024.0f * 1024.0f);

    auto mem_chart = ui::area_chart();
    mem_chart->set_title("Memory Usage");
    mem_chart->set_y_unit_suffix(" GB");
    mem_chart->set_y_range(0.0f, total_gb);
    mem_chart->set_show_legend(false);
    auto mem_chart_ptr = mem_chart.get();

    auto network_chart = ui::area_chart();
    network_chart->set_title("Network Throughput");
    network_chart->set_y_unit_suffix(" Mbps");
    network_chart->set_show_legend(false);
    auto network_chart_ptr = network_chart.get();

    // Grid of per-core graphs (Windows Task Manager's "Logical processors"
    // view). hwinfo::monitoring::cpu::Data already carries per-thread
    // utilization alongside the aggregate, so no extra sampling is needed --
    // see the cpu_monitor callback below, which feeds both.
    //
    // hwinfo::getAllCPUs()[0].numLogicalCores() undercounts on at least some
    // Linux systems (reports 4 on an 8-thread machine here), so the core
    // count comes from an actual thread_utilization sample instead -- that's
    // the same vector the per-core data itself comes from, so it can't
    // disagree with what's actually fed into the sparklines below.
    static auto core_sparklines = std::vector<CoreSparkline *>{};
    auto num_cores =
        static_cast<unsigned>(hwinfo::monitoring::cpu::thread_utilization().size());
    auto cols = num_cores > 0
                   ? static_cast<unsigned>(std::ceil(std::sqrt(static_cast<double>(num_cores))))
                   : 1u;

    auto cores_grid = ui::vbox();
    for (auto row_start = 0u; row_start < num_cores; row_start += cols) {
        auto row = ui::hbox();
        for (auto i = row_start; i < std::min(row_start + cols, num_cores); i++) {
            auto spark = std::make_unique<CoreSparkline>(static_cast<int>(i));
            core_sparklines.push_back(spark.get());
            row.w->add_widget(std::move(spark), 1, toolkit::Alignment::Fill);
        }
        cores_grid.w->add_widget(std::move(row.w), 1);
    }
    // No ScrollArea wrapping: stretch=1 above already makes the grid fill the
    // tab exactly, and a ScrollArea sizes its scroll range off the content's
    // *natural* (size_hint) size rather than what stretch grows it to --
    // showing a scrollbar even when everything already fits on screen.
    auto cores_view = std::move(cores_grid);

    auto process_model = std::make_shared<toolkit::StringTableModel>(
        std::vector<std::string>{"PID", "Name", "CPU %", "Memory (MB)", "Threads", "State"});
    auto process_table = ui::table_view(process_model);
    process_table->set_alternating_row_colors(true);

    // PID/CPU %/Memory (MB)/Threads are all plain numbers ("9" vs "10"), so
    // sort them by value instead of TableView's default lexicographic compare.
    auto const numeric_less = [](std::string_view a, std::string_view b) {
        return std::strtod(std::string(a).c_str(), nullptr) <
               std::strtod(std::string(b).c_str(), nullptr);
    };
    process_table->set_column_comparator(0, numeric_less); // PID
    process_table->set_column_comparator(2, numeric_less); // CPU %
    process_table->set_column_comparator(3, numeric_less); // Memory (MB)
    process_table->set_column_comparator(4, numeric_less); // Threads
    process_table->set_row_markdown_tooltip_provider([process_model](size_t model_row) -> std::string {
        auto const &pid_text = process_model->cell_text(model_row, 0);
        return process_info::format_tooltip(
            process_info::get_process_detail(std::atoi(pid_text.c_str())));
    });

    // ContextMenu::show() stashes `this` into the window's popup, so it must
    // outlive the popup -- a function-local static (like the *_series charts
    // below) gives it the same "lives for the app's run" lifetime, and simply
    // gets replaced on the next right click.
    static auto process_context_menu = std::unique_ptr<toolkit::ContextMenu>{};
    process_table->set_row_context_menu_handler(
        [weak_window = std::weak_ptr(window), process_model](size_t model_row,
                                                             toolkit::Point window_pos) {
            auto const &pid_text = process_model->cell_text(model_row, 0);
            auto pid = std::atoi(pid_text.c_str());

            auto signal_menu = std::make_shared<toolkit::Menu>("Send Signal");
            for (auto const &sig : process_info::available_signals()) {
                signal_menu->add_action(sig.name, [pid, value = sig.value] {
                    process_info::send_signal(pid, value);
                });
            }

            auto items = std::vector<toolkit::MenuItem>{};
            items.push_back(toolkit::MenuItem::submenu_item("Send Signal", signal_menu));
            items.push_back(toolkit::MenuItem::sep());
            items.push_back(toolkit::MenuItem::action("Goto Exe", [] {
                // TODO: reveal process_info::get_process_detail(pid).exe_path in
                // the platform's file manager (xdg-open/explorer /select).
            }));
            items.push_back(toolkit::MenuItem::sep());
            items.push_back(toolkit::MenuItem::action("Copy Details", [pid] {
                auto text = process_info::format_tooltip(process_info::get_process_detail(pid));
                toolkit::Clipboard::set_text(text);
            }));

            auto window = weak_window.lock();
            if (!window) {
                return;
            }
            process_context_menu = std::make_unique<toolkit::ContextMenu>(std::move(items));
            process_context_menu->show(window.get(), window_pos);
        });

    auto process_filter_input = ui::line_input("Filter by name...");
    auto *process_filter_input_ptr = process_filter_input.get();

    auto processes_tab =
        ui::vbox().add(process_filter_input).add(std::move(process_table), ui::expand);

    auto tabs = ui::tab_widget()
                    .orientation(toolkit::TabOrientation::WestVertical)
                    .tabs_closable(false)
                    .tabs_movable(false)
                    .add_tab("CPU", std::move(cpu_chart))
                    .add_tab("CPU Cores", std::move(cores_view))
                    .add_tab("Memory", std::move(mem_chart))
                    .add_tab("Network", std::move(network_chart))
                    .add_tab("Processes", std::move(processes_tab));
    window->set_root(std::move(tabs));

    static auto cpu_series =
        SlidingChart{cpu_chart_ptr, window, "CPU", toolkit::Color::rgb(0.20f, 0.55f, 0.90f),
                     toolkit::Color::rgba(0.20f, 0.55f, 0.90f, 0.25f)};
    static auto mem_series =
        SlidingChart{mem_chart_ptr, window, "Memory", toolkit::Color::rgb(0.60f, 0.30f, 0.85f),
                     toolkit::Color::rgba(0.60f, 0.30f, 0.85f, 0.25f)};
    static auto network_series =
        SlidingChart{network_chart_ptr, window, "Network", toolkit::Color::rgb(0.20f, 0.75f, 0.35f),
                     toolkit::Color::rgba(0.20f, 0.75f, 0.35f, 0.25f)};

    // Memory reads are cheap syscalls (no artificial delay), so poll it
    // directly from the toolkit's own timer on the main thread.
    //
    // last_network_bytes is a static local (not a `mutable` capture) so it
    // survives regardless of how the platform stores/invokes this callback --
    // /proc/net/dev's counters only ever grow, so without remembering the
    // previous reading here, `delta` would silently become "total bytes
    // since app start" instead of "bytes since the last tick".
    auto sample_timer = window->start_timer(kSampleIntervalSec, [total_bytes] {
        auto mem_data = hwinfo::monitoring::ram::fetch();
        auto used_gb = total_bytes > 0 ? static_cast<float>(total_bytes - mem_data.available_bytes) /
                                              (1024.0f * 1024.0f * 1024.0f)
                                       : 0.0f;
        mem_series.append(used_gb);

        static auto last_network_bytes = process_info::total_network_bytes();
        auto now_network_bytes = process_info::total_network_bytes();
        auto delta_bytes = now_network_bytes >= last_network_bytes
                               ? now_network_bytes - last_network_bytes
                               : 0;
        last_network_bytes = now_network_bytes;
        auto mbps =
            static_cast<float>(delta_bytes) * 8.0f / (1000.0f * 1000.0f) / kSampleIntervalSec;
        network_series.append(mbps);
    });

    // hwinfo::monitoring::cpu::fetch() blocks for its `sleep` argument
    // (default 200ms) to measure a utilization delta, so it runs on its own
    // background thread via hwinfo's Monitor; the result is marshaled onto
    // the main thread before touching any widget, same pattern as
    // toolkit::net::fetch_async.
    auto cpu_monitor = hwinfo::monitoring::cpu::Monitor(
        [] { return hwinfo::monitoring::cpu::fetch(); },
        [](hwinfo::monitoring::cpu::Data const &data) {
            auto utilization_percent = static_cast<float>(data.utilization) * 100.0f;
            auto thread_utilization = data.thread_utilization;
            toolkit::Application::post_to_main_thread([utilization_percent, thread_utilization] {
                cpu_series.append(utilization_percent);
                for (size_t i = 0; i < thread_utilization.size() && i < core_sparklines.size(); i++) {
                    core_sparklines[i]->append(static_cast<float>(thread_utilization[i]) * 100.0f);
                }
            });
        },
        std::chrono::milliseconds(static_cast<int>(kSampleIntervalSec * 1000)));
    cpu_monitor.start();

    // Process listing is comparatively expensive (reads several /proc files
    // per PID), so it refreshes on its own slower timer rather than piggybacking
    // on the 1s chart sample tick.
    //
    // The filter box needs to re-render instantly on every keystroke without
    // waiting for the next refresh tick, so the last sampled rows and the
    // current filter text are kept around and both paths funnel through the
    // same render_process_table.
    auto latest_process_rows = std::vector<process_info::ProcessRow>{};
    auto process_filter = std::string{};
    auto render_process_table = [process_model](std::vector<process_info::ProcessRow> const &rows,
                                                std::string const &filter) {
        auto lower_filter = filter;
        std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);

        auto table_rows = std::vector<std::vector<std::string>>{};
        table_rows.reserve(rows.size());
        for (auto const &r : rows) {
            if (!lower_filter.empty()) {
                auto lower_name = r.name;
                std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                if (lower_name.find(lower_filter) == std::string::npos) {
                    continue;
                }
            }
            table_rows.push_back({
                std::to_string(r.pid),
                r.name,
                fmt::format("{:.1f}", r.cpu_percent),
                fmt::format("{:.1f}", static_cast<float>(r.rss_kb) / 1024.0f),
                std::to_string(r.threads),
                std::string(r.state),
            });
        }
        process_model->set_data(
            {"PID", "Name", "CPU %", "Memory (MB)", "Threads", "State"}, std::move(table_rows));
    };

    process_filter_input_ptr->on_change = [&process_filter, &latest_process_rows,
                                           render_process_table,
                                           window](std::string const &text, toolkit::LineInput &) {
        process_filter = text;
        render_process_table(latest_process_rows, process_filter);
        window->request_redraw();
    };

    auto prev_cpu_ticks = std::unordered_map<int, uint64_t>{};
    auto process_timer = window->start_timer(
        kProcessRefreshIntervalSec,
        [window, &prev_cpu_ticks, &latest_process_rows, &process_filter, render_process_table] {
            latest_process_rows =
                process_info::list_processes(prev_cpu_ticks, kProcessRefreshIntervalSec);
            render_process_table(latest_process_rows, process_filter);
            window->request_redraw();
        });

    window->on_key = [&app](toolkit::KeyEvent const &e) -> bool {
        if (e.type == toolkit::KeyEvent::Type::Press && e.super && e.text == "q") {
            app.quit();
            return true;
        }
        return false;
    };

    auto tray_actions = std::vector<toolkit::Command::Ptr>{};
    auto quit_cmd = toolkit::Command::create("Quit", [&app] { app.quit(); });
    quit_cmd->set_icon("application-exit");
    tray_actions.push_back(std::move(quit_cmd));

    // icon() is the bitmap every backend draws; icon_name() is the extra hint a
    // Linux tray host uses to theme and size it to the panel.
    auto tray = toolkit::TrayIcon::builder()
                    .icon(app.load_icon("utilities-system-monitor", 22, ""))
                    .icon_name("utilities-system-monitor")
                    .tooltip("Task Manager")
                    .id("svision3-task-manager")
                    .owner_window(window)
                    .actions(std::move(tray_actions))
                    .build();
    if (!tray) {
        spdlog::info("Tray icon unavailable (no D-Bus session bus?) -- continuing without it");
    }

    window->show();

    if (!screenshot_path.empty()) {
        // Let a few real samples accumulate before capturing.
        window->start_timer(
            3.5f,
            [&app, weak_window = std::weak_ptr(window), screenshot_path] {
                auto window = weak_window.lock();
                auto ok = window && window->save_to_png(screenshot_path);
                spdlog::info("Screenshot saved to '{}': {}", screenshot_path,
                             ok ? "success" : "failed");
                app.quit();
            },
            false);
    }

    return app.run();
}
