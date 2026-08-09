#include "declarative_charts.hpp"
#include "toolkit/application.hpp"
#include "toolkit/net/http.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

static auto current_style = toolkit::ThemeStyle::MacOS;
static auto current_scheme = toolkit::ColorScheme::Light;

static void apply_theme(toolkit::Window *window) {
    toolkit::Theme::set_current(toolkit::ThemeFactory::create(current_style, current_scheme));
    window->request_redraw();
}

static constexpr auto series_colors = std::array<toolkit::Color, 6>{
    toolkit::Color::rgb(0.30f, 0.69f, 0.29f), toolkit::Color::rgb(0.22f, 0.56f, 0.94f),
    toolkit::Color::rgb(0.91f, 0.30f, 0.24f), toolkit::Color::rgb(0.96f, 0.73f, 0.18f),
    toolkit::Color::rgb(0.61f, 0.35f, 0.71f), toolkit::Color::rgb(0.94f, 0.50f, 0.17f),
};
static constexpr auto kNumColors = static_cast<int>(series_colors.size());
static auto next_color = 0;

struct StockDay {
    std::string date;
    float open = 0;
    float high = 0;
    float low = 0;
    float close = 0;
    float volume = 0;
};

static std::string format_timestamp(int64_t ts) {
    auto t = static_cast<time_t>(ts);
    auto tm_buf = tm{};
    // gmtime_r is POSIX; MSVC spells it gmtime_s and takes the arguments the
    // other way round. Both are the thread-safe form -- plain gmtime() returns
    // a shared static buffer.
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_buf);
    return buf;
}

static std::vector<StockDay> parse_yahoo_json(std::string const &body) {
    auto days = std::vector<StockDay>{};
    try {
        auto j = nlohmann::json::parse(body);
        auto const &result = j["chart"]["result"][0];
        auto const &timestamps = result["timestamp"];
        auto const &quote = result["indicators"]["quote"][0];
        auto const &opens = quote["open"];
        auto const &highs = quote["high"];
        auto const &lows = quote["low"];
        auto const &closes = quote["close"];
        auto const &volumes = quote["volume"];

        for (auto i = size_t{0}; i < timestamps.size(); i++) {
            auto day = StockDay{};
            day.date = format_timestamp(timestamps[i].get<int64_t>());
            if (closes[i].is_null()) {
                continue;
            }
            day.open = opens[i].is_null() ? 0 : opens[i].get<float>();
            day.high = highs[i].is_null() ? 0 : highs[i].get<float>();
            day.low = lows[i].is_null() ? 0 : lows[i].get<float>();
            day.close = closes[i].get<float>();
            day.volume = volumes[i].is_null() ? 0 : volumes[i].get<float>();
            days.push_back(std::move(day));
        }
    } catch (std::exception const &e) {
        spdlog::error("JSON parse error: {}", e.what());
    }
    return days;
}

static std::string build_yahoo_url(std::string const &ticker, std::string const &range) {
    return "https://query1.finance.yahoo.com/v8/finance/chart/" + ticker + "?range=" + range +
           "&interval=1d";
}

struct TimeRange {
    std::string label;
    std::string api_range;
};

static const auto time_ranges = std::vector<TimeRange>{
    {"1M", "1mo"}, {"3M", "3mo"}, {"6M", "6mo"}, {"1Y", "1y"}, {"5Y", "5y"},
};

static auto current_range = 3; // default: 1Y

int main() {
    spdlog::set_level(spdlog::level::debug);

    toolkit::Theme::set_current(toolkit::ThemeFactory::create(current_style, current_scheme));

    auto app = toolkit::Application{};
    auto window = app.create_window("Stock Viewer", {960, 640});

    // -- Charts, built up front so we can wire their raw pointers into the
    // async fetch/reload closures before moving each Element into the tab widget --
    auto price_chart = ui::line_chart();
    price_chart->set_title("Stock Price");
    price_chart->set_y_label("Price (USD)");
    price_chart->set_x_label("Trading Day");
    auto price_chart_ptr = price_chart.get();

    auto volume_chart = ui::bar_chart();
    volume_chart->set_title("Trading Volume");
    volume_chart->set_y_label("Volume");
    volume_chart->set_x_label("Trading Day");
    auto volume_chart_ptr = volume_chart.get();

    auto candle_chart = ui::candlestick_chart();
    candle_chart->set_title("Candlestick (OHLC)");
    candle_chart->set_y_label("Price (USD)");
    candle_chart->set_x_label("Trading Day");
    auto candle_chart_ptr = candle_chart.get();

    auto returns_chart = ui::area_chart();
    returns_chart->set_title("Normalized Returns");
    returns_chart->set_y_label("Return (%)");
    returns_chart->set_x_label("Trading Day");
    auto returns_chart_ptr = returns_chart.get();

    auto pie_chart = ui::pie_chart();
    pie_chart->set_title("Portfolio Allocation");
    pie_chart->set_donut(true);
    auto pie_chart_ptr = pie_chart.get();

    auto scatter = ui::scatter_plot();
    scatter->set_title("Daily Return Correlation");
    auto scatter_ptr = scatter.get();

    auto histogram = ui::histogram();
    histogram->set_title("Daily Return Distribution");
    histogram->set_x_label("Daily Return (%)");
    histogram->set_y_label("Frequency");
    histogram->set_bin_count(25);
    auto histogram_ptr = histogram.get();

    auto stacked_vol = ui::stacked_bar_chart();
    stacked_vol->set_title("Stacked Volume");
    stacked_vol->set_y_label("Volume");
    stacked_vol->set_x_label("Trading Day");
    auto stacked_vol_ptr = stacked_vol.get();

    auto status = ui::label("Enter a ticker symbol and click Add").shrinkable(true);
    auto status_ptr = status.get();

    auto progress = ui::progress_bar(0.5f).visible(false);
    auto progress_ptr = progress.get();

    auto ticker_input = ui::line_input("Ticker (e.g. AAPL)");
    auto ticker_input_ptr = ticker_input.get();

    auto range_btns = std::vector<toolkit::Button *>{};
    auto loaded_tickers = std::vector<std::string>{};

    struct TickerReturns {
        std::string name;
        toolkit::Color color;
        std::vector<float> daily_returns;
        float latest_price = 0;
    };
    auto all_returns = std::vector<TickerReturns>{};

    // -- Helper: fetch a ticker and add to all charts --
    auto fetch_ticker = [&](std::string ticker) {
        std::transform(ticker.begin(), ticker.end(), ticker.begin(), ::toupper);
        if (ticker.empty()) {
            return;
        }

        auto url = build_yahoo_url(ticker, time_ranges[current_range].api_range);

        spdlog::info("Fetching {}", url);
        if (progress_ptr) {
            progress_ptr->show();
        }
        if (status_ptr) {
            status_ptr->set_text("Loading " + ticker + "...");
        }
        if (window) {
            window->request_redraw();
        }

        auto req = toolkit::net::HttpRequest{};
        req.url = url;
        req.headers.push_back({"User-Agent", "Mozilla/5.0"});

        toolkit::net::fetch_async(req, [ticker, price_chart_ptr, volume_chart_ptr, candle_chart_ptr,
                                        returns_chart_ptr, pie_chart_ptr, scatter_ptr,
                                        histogram_ptr, stacked_vol_ptr, status_ptr, progress_ptr,
                                        window, &loaded_tickers,
                                        &all_returns](toolkit::net::HttpResponse resp) {
            if (progress_ptr) {
                progress_ptr->hide();
            }

            if (!resp.ok()) {
                spdlog::error("HTTP error for {}: {} (status {})", ticker, resp.error,
                              resp.status_code);
                if (status_ptr) {
                    status_ptr->set_text(
                        ticker + ": fetch failed (" +
                        (resp.error.empty() ? std::to_string(resp.status_code) : resp.error) + ")");
                }
                if (window) {
                    window->request_redraw();
                }
                return;
            }

            auto days = parse_yahoo_json(resp.body);
            if (days.empty()) {
                if (status_ptr) {
                    status_ptr->set_text(ticker + ": no data returned");
                }
                if (window) {
                    window->request_redraw();
                }
                return;
            }

            auto color = series_colors[next_color % kNumColors];
            next_color++;

            // Price line chart
            if (price_chart_ptr) {
                auto series = toolkit::ChartSeries{};
                series.name = ticker;
                series.color = color;
                for (auto i = size_t{0}; i < days.size(); i++) {
                    auto dp = toolkit::ChartDataPoint{};
                    dp.x = static_cast<float>(i);
                    dp.y = days[i].close;
                    dp.label = days[i].date;
                    series.points.push_back(std::move(dp));
                }
                price_chart_ptr->add_series(std::move(series));
            }

            // Volume bar chart
            if (volume_chart_ptr) {
                auto vol_series = toolkit::BarSeries{};
                vol_series.name = ticker;
                vol_series.default_color = color;
                for (auto i = size_t{0}; i < days.size(); i++) {
                    auto bp = toolkit::BarDataPoint{};
                    bp.x = static_cast<float>(i);
                    bp.y = days[i].volume;
                    bp.label = days[i].date;
                    auto up = days[i].close >= days[i].open;
                    bp.color = up ? toolkit::Color::rgba(0.30f, 0.69f, 0.29f, 0.7f)
                                  : toolkit::Color::rgba(0.91f, 0.30f, 0.24f, 0.7f);
                    bp.detail = up ? "Close > Open (Up)" : "Close < Open (Down)";
                    vol_series.points.push_back(std::move(bp));
                }
                volume_chart_ptr->add_series(std::move(vol_series));
            }

            // Candlestick chart
            if (candle_chart_ptr) {
                auto cs = toolkit::CandlestickSeries{};
                cs.name = ticker;
                cs.up_color = color.lighten(0.1f);
                cs.down_color = color.darken(0.1f);
                for (auto i = size_t{0}; i < days.size(); i++) {
                    auto cd = toolkit::CandlestickData{};
                    cd.x = static_cast<float>(i);
                    cd.open = days[i].open;
                    cd.high = days[i].high;
                    cd.low = days[i].low;
                    cd.close = days[i].close;
                    cd.label = days[i].date;
                    cs.candles.push_back(std::move(cd));
                }
                candle_chart_ptr->add_series(std::move(cs));
            }

            // Compute daily returns for this ticker
            auto daily_ret = std::vector<float>{};
            daily_ret.reserve(days.size());
            for (auto i = size_t{1}; i < days.size(); i++) {
                if (days[i - 1].close > 0) {
                    daily_ret.push_back((days[i].close - days[i - 1].close) / days[i - 1].close *
                                        100.0f);
                }
            }

            // Area chart: normalized returns (% change from day 0)
            if (returns_chart_ptr && !days.empty()) {
                auto as = toolkit::AreaSeries{};
                as.name = ticker;
                as.line_color = color;
                as.fill_color = toolkit::Color::rgba(color.r, color.g, color.b, 0.2f);
                auto base = days[0].close;
                if (base > 0) {
                    for (auto i = size_t{0}; i < days.size(); i++) {
                        auto dp = toolkit::AreaDataPoint{};
                        dp.x = static_cast<float>(i);
                        dp.y = (days[i].close - base) / base * 100.0f;
                        dp.label = days[i].date;
                        as.points.push_back(std::move(dp));
                    }
                }
                returns_chart_ptr->add_series(std::move(as));
            }

            // Store returns for scatter correlation and pie
            auto tr = TickerReturns{};
            tr.name = ticker;
            tr.color = color;
            tr.daily_returns = daily_ret;
            tr.latest_price = days.back().close;
            all_returns.push_back(std::move(tr));

            // Pie chart: portfolio allocation by latest price
            if (pie_chart_ptr) {
                auto slices = std::vector<toolkit::PieSlice>{};
                for (auto const &r : all_returns) {
                    slices.push_back({r.name, r.latest_price, r.color});
                }
                pie_chart_ptr->set_slices(std::move(slices));
            }

            // Scatter plot: correlation of daily returns (first two tickers)
            if (scatter_ptr && all_returns.size() >= 2) {
                scatter_ptr->clear_series();
                auto const &r0 = all_returns[0];
                auto const &r1 = all_returns[1];
                auto n = std::min(r0.daily_returns.size(), r1.daily_returns.size());
                if (n > 0) {
                    auto ss = toolkit::ScatterSeries{};
                    ss.name = r0.name + " vs " + r1.name;
                    ss.color = toolkit::Color::rgb(0.40f, 0.65f, 0.95f);
                    ss.point_radius = 3.5f;
                    for (auto i = size_t{0}; i < n; i++) {
                        auto sp = toolkit::ScatterPoint{};
                        sp.x = r0.daily_returns[i];
                        sp.y = r1.daily_returns[i];
                        ss.points.push_back(std::move(sp));
                    }
                    scatter_ptr->add_series(std::move(ss));
                    scatter_ptr->set_x_label(r0.name + " Daily Return (%)");
                    scatter_ptr->set_y_label(r1.name + " Daily Return (%)");
                }
            }

            // Histogram: distribution of daily returns
            if (histogram_ptr && !daily_ret.empty()) {
                auto hs = toolkit::HistogramSeries{};
                hs.name = ticker;
                hs.color = color;
                hs.values = daily_ret;
                histogram_ptr->add_series(std::move(hs));
            }

            // Stacked bar: volume per ticker stacked by day
            if (stacked_vol_ptr) {
                if (stacked_vol_ptr->series_count() == 0) {
                    auto cats = std::vector<toolkit::StackedBarCategory>{};
                    auto label_step = std::max(1, static_cast<int>(days.size()) / 12);
                    for (auto i = size_t{0}; i < days.size(); i++) {
                        auto cat = toolkit::StackedBarCategory{};
                        cat.x = static_cast<float>(i);
                        cat.label = (i % label_step == 0) ? days[i].date : "";
                        cats.push_back(std::move(cat));
                    }
                    stacked_vol_ptr->set_categories(std::move(cats));
                }
                auto sbs = toolkit::StackedBarSeries{};
                sbs.name = ticker;
                sbs.color = color;
                sbs.values.reserve(days.size());
                for (auto const &d : days) {
                    sbs.values.push_back(d.volume);
                }
                stacked_vol_ptr->add_series(std::move(sbs));
            }

            loaded_tickers.push_back(ticker);

            auto const &last = days.back();
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "%s | Open: %.2f | Close: %.2f | High: %.2f | Low: "
                          "%.2f | Vol: %.0f",
                          ticker.c_str(), static_cast<double>(last.open),
                          static_cast<double>(last.close), static_cast<double>(last.high),
                          static_cast<double>(last.low), static_cast<double>(last.volume));
            if (status_ptr) {
                status_ptr->set_text(buf);
            }
            if (window) {
                window->request_redraw();
            }
        });
    };

    // -- Helper: reload all tickers with new range --
    auto reload_all = [&]() {
        if (price_chart_ptr) {
            price_chart_ptr->clear_series();
        }
        if (volume_chart_ptr) {
            volume_chart_ptr->clear_series();
        }
        if (candle_chart_ptr) {
            candle_chart_ptr->clear_series();
        }
        if (returns_chart_ptr) {
            returns_chart_ptr->clear_series();
        }
        if (pie_chart_ptr) {
            pie_chart_ptr->clear();
        }
        if (scatter_ptr) {
            scatter_ptr->clear_series();
        }
        if (histogram_ptr) {
            histogram_ptr->clear_series();
        }
        if (stacked_vol_ptr) {
            stacked_vol_ptr->clear();
        }
        all_returns.clear();
        next_color = 0;
        auto tickers_copy = loaded_tickers;
        loaded_tickers.clear();
        for (auto const &t : tickers_copy) {
            fetch_ticker(t);
        }
    };

    // -- Build UI --
    ticker_input->on_submit = [&fetch_ticker](std::string const &text, toolkit::LineInput &) {
        fetch_ticker(text);
    };
    auto add_btn = ui::button("Add").on_click(
        [&fetch_ticker, ticker_input_ptr] { fetch_ticker(ticker_input_ptr->text()); });

    auto toolbar = ui::hbox().margins({8, 12, 8, 12}).spacing(8);
    toolbar.add_child(std::move(ticker_input), 1);
    toolbar.add_child(std::move(add_btn));

    // Time range buttons
    for (auto i = 0; i < static_cast<int>(time_ranges.size()); i++) {
        auto btn = ui::button(time_ranges[i].label).on_click([i, &reload_all] {
            current_range = i;
            reload_all();
        });
        range_btns.push_back(btn.get());
        toolbar.add_child(std::move(btn));
    }

    toolbar.add_child(ui::label("Theme:"));

    auto style_names = std::vector<std::string>{};
    for (auto i = 0; i < toolkit::theme_style_count; i++) {
        style_names.push_back(toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i)));
    }
    auto theme_combo = ui::combobox(style_names)
                           .selected(static_cast<int>(current_style))
                           .on_change([weak_window = std::weak_ptr(window)](int index) {
                               current_style = static_cast<toolkit::ThemeStyle>(index);
                               if (auto window = weak_window.lock()) {
                                   apply_theme(window.get());
                               }
                           });
    toolbar.add_child(std::move(theme_combo));

    auto quit_btn = ui::button("Quit").on_click([&app] { app.quit(); });
    toolbar.add_child(std::move(quit_btn));

    auto tabs = ui::tab_widget()
                    .tabs_closable(false)
                    .add_tab("Price", std::move(price_chart))
                    .add_tab("Volume", std::move(volume_chart))
                    .add_tab("Candlestick", std::move(candle_chart))
                    .add_tab("Returns", std::move(returns_chart))
                    .add_tab("Allocation", std::move(pie_chart))
                    .add_tab("Correlation", std::move(scatter))
                    .add_tab("Distribution", std::move(histogram))
                    .add_tab("Stacked Vol", std::move(stacked_vol));

    auto status_row = ui::hbox().margins({4, 12, 4, 12});
    status_row.add_child(std::move(status), 1);

    auto root = ui::vbox().margins({0, 0, 0, 0}).spacing(0);
    root.add_child(std::move(toolbar));
    root.add_child(std::move(progress));
    root.add_child(std::move(tabs), 1);
    root.add_child(std::move(status_row));

    window->set_root(std::move(root));
    window->on_key = [&app](toolkit::KeyEvent const &e) -> bool {
        if (e.type == toolkit::KeyEvent::Type::Press && e.super && e.text == "q") {
            app.quit();
            return true;
        }
        return false;
    };
    window->show();

    fetch_ticker("AAPL");
    fetch_ticker("IBM");

    return app.run();
}
