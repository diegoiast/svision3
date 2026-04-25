// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/item_model.hpp"
#include "toolkit/application.hpp"
#include "toolkit/stopwatch.hpp"
#include <algorithm>
#include <cctype>
#include <thread>

#include <spdlog/spdlog.h>

namespace toolkit {

// ── StringListModel ──────────────────────────────────────────────────────────

StringListModel::StringListModel(std::vector<std::string> items) : items_(std::move(items)) {}

std::string StringListModel::cell_text(size_t row, size_t /*col*/) const {
    if (row >= items_.size()) {
        return {};
    }
    return items_[row];
}

void StringListModel::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    if (on_data_changed) {
        on_data_changed();
    }
}

void StringListModel::append(std::string item) {
    items_.push_back(std::move(item));
    if (on_data_changed) {
        on_data_changed();
    }
}

void StringListModel::remove(size_t index) {
    if (index < items_.size()) {
        items_.erase(items_.begin() + static_cast<ptrdiff_t>(index));
        if (on_data_changed) {
            on_data_changed();
        }
    }
}

// ── StringTableModel ─────────────────────────────────────────────────────────

StringTableModel::StringTableModel(std::vector<std::string> headers,
                                   std::vector<std::vector<std::string>> rows)
    : headers_(std::move(headers)), rows_(std::move(rows)) {}

std::string StringTableModel::cell_text(size_t row, size_t col) const {
    if (row >= rows_.size()) {
        return {};
    }
    auto const &r = rows_[row];
    if (col >= r.size()) {
        return {};
    }
    return r[col];
}

std::string StringTableModel::header_text(size_t col) const {
    if (col >= headers_.size()) {
        return {};
    }
    return headers_[col];
}

void StringTableModel::set_data(std::vector<std::string> headers,
                                std::vector<std::vector<std::string>> rows) {
    headers_ = std::move(headers);
    rows_ = std::move(rows);
    if (on_data_changed) {
        on_data_changed();
    }
}

void StringTableModel::append_row(std::vector<std::string> row) {
    rows_.push_back(std::move(row));
    if (on_data_changed) {
        on_data_changed();
    }
}

void StringTableModel::remove_row(size_t index) {
    if (index < rows_.size()) {
        rows_.erase(rows_.begin() + static_cast<ptrdiff_t>(index));
        if (on_data_changed) {
            on_data_changed();
        }
    }
}

StandardIconModel::StandardIconModel(std::vector<StandardIconItem> items)
    : items_(std::move(items)) {}

std::string StandardIconModel::cell_text(size_t row, size_t /*col*/) const {
    if (row >= items_.size()) {
        return {};
    }
    return items_[row].text;
}

std::string StandardIconModel::tooltip(size_t row) const {
    if (row >= items_.size()) {
        return {};
    }
    auto const &item = items_[row];
    return item.tooltip_text.empty() ? item.text : item.tooltip_text;
}

Icon StandardIconModel::icon_at(size_t row, size_t /*col*/, int size) const {
    if (row >= items_.size()) {
        return nullptr;
    }
    auto &item = items_[row];
    if (item.cached_icon && item.cached_size == size) {
        return item.cached_icon;
    }
    item.cached_icon = Application::instance().load_icon(item.icon_name, size, item.icon_category);
    item.cached_size = size;
    return item.cached_icon;
}

void StandardIconModel::set_items(std::vector<StandardIconItem> items) {
    items_ = std::move(items);
    if (on_data_changed) {
        on_data_changed();
    }
}

void StandardIconModel::append(StandardIconItem item) {
    items_.push_back(std::move(item));
    if (on_data_changed) {
        on_data_changed();
    }
}

// ── FilterAdapter ─────────────────────────────────────────────────────────────

static bool contains_icase(std::string const &hay, std::string const &needle) {
    if (needle.empty()) {
        return true;
    }
    auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
    return it != hay.end();
}

FilterAdapter::FilterAdapter(std::shared_ptr<ItemModel> source) : source_(std::move(source)) {
    rebuild_sync();
    if (source_) {
        source_->on_data_changed = [this] {
            rebuild_sync();
            if (on_data_changed) {
                on_data_changed();
            }
        };
    }
}

FilterAdapter::~FilterAdapter() { generation_->fetch_add(1); }

std::string FilterAdapter::cell_text(size_t row, size_t col) const {
    if (row >= indices_.size() || !source_) {
        return {};
    }
    return source_->cell_text(indices_[row], col);
}

std::string FilterAdapter::header_text(size_t col) const {
    return source_ ? source_->header_text(col) : std::string{};
}

Icon FilterAdapter::icon_at(size_t row, size_t col, int size) const {
    if (row >= indices_.size() || !source_) {
        return nullptr;
    }
    return source_->icon_at(indices_[row], col, size);
}

std::string FilterAdapter::tooltip(size_t row) const {
    if (row >= indices_.size() || !source_) {
        return {};
    }
    return source_->tooltip(indices_[row]);
}

std::optional<size_t> FilterAdapter::source_index(size_t filtered_index) const {
    if (filtered_index >= indices_.size()) {
        return std::nullopt;
    }
    return indices_[filtered_index];
}

void FilterAdapter::set_filter(std::string const &filter) {
    filter_ = filter;
    if (delay_per_item_ms_ > 0) {
        rebuild_async();
    } else {
        rebuild_sync();
        if (on_data_changed) {
            on_data_changed();
        }
    }
}

void FilterAdapter::rebuild_sync() {
    indices_.clear();
    if (!source_) {
        return;
    }
    auto n = source_->row_count();
    for (size_t i = 0; i < n; i++) {
        if (contains_icase(source_->cell_text(i, 0), filter_)) {
            indices_.push_back(i);
        }
    }
}

void FilterAdapter::rebuild_async() {
    auto gen = generation_->fetch_add(1) + 1;

    if (on_progress) {
        on_progress(0.0f);
    }

    auto source = source_;
    auto filter = filter_;
    auto generation = generation_;
    auto delay_ms = delay_per_item_ms_;
    auto self = shared_from_this();

    std::thread([self, source, filter, generation, gen, delay_ms]() {
        Stopwatch sw;
        std::vector<size_t> result;

        if (source) {
            auto n = source->row_count();
            auto last_percentage = -1;
            for (size_t i = 0; i < n; i++) {
                if (generation->load() != gen) {
                    return;
                }

                if (contains_icase(source->cell_text(i, 0), filter)) {
                    result.push_back(i);
                }

                auto percentage = (n > 0) ? ((i + 1) * 100 / n) : 100;
                if (static_cast<int>(percentage) != last_percentage) {
                    auto progress = static_cast<float>(i + 1) / static_cast<float>(n);
                    last_percentage = static_cast<int>(percentage);
                    Application::post_to_main_thread([self, generation, gen, progress]() {
                        if (generation->load() != gen) {
                            return;
                        }
                        if (self->on_progress) {
                            self->on_progress(progress);
                        }
                    });
                }

                if (delay_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                }
            }
        }

        if (generation->load() != gen) {
            return;
        }

        auto elapsed = sw.elapsed_ms();
        spdlog::info("Filter '{}': {} results in {:.2f} ms", filter, result.size(), elapsed);

        Application::post_to_main_thread([self, generation, gen, result = std::move(result)]() {
            if (generation->load() != gen) {
                return;
            }
            self->indices_ = std::move(result);
            if (self->on_progress) {
                self->on_progress(-1.0f);
            }
            if (self->on_data_changed) {
                self->on_data_changed();
            }
        });
    }).detach();
}

} // namespace toolkit
