// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image_loader.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace toolkit {

class Widget;

class ItemModel {
  public:
    virtual ~ItemModel() = default;

    virtual size_t row_count() const = 0;
    virtual size_t column_count() const { return 1; }
    virtual std::string cell_text(size_t row, size_t col) const = 0;
    virtual std::string header_text(size_t col) const { return {}; }
    virtual Icon icon_at(size_t row, size_t col, int size) const { return nullptr; }
    virtual std::string tooltip(size_t row) const { return cell_text(row, 0); }

    std::function<void()> on_data_changed;
};

class StringListModel : public ItemModel {
  public:
    explicit StringListModel(std::vector<std::string> items = {});

    size_t row_count() const override { return items_.size(); }
    std::string cell_text(size_t row, size_t col) const override;

    void set_items(std::vector<std::string> items);
    void append(std::string item);
    void remove(size_t index);

  private:
    std::vector<std::string> items_;
};

class StringTableModel : public ItemModel {
  public:
    explicit StringTableModel(std::vector<std::string> headers = {},
                              std::vector<std::vector<std::string>> rows = {});

    size_t row_count() const override { return rows_.size(); }
    size_t column_count() const override { return headers_.size(); }
    std::string cell_text(size_t row, size_t col) const override;
    std::string header_text(size_t col) const override;

    void set_data(std::vector<std::string> headers, std::vector<std::vector<std::string>> rows);
    void append_row(std::vector<std::string> row);
    void remove_row(size_t index);

  private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
};

struct StandardIconItem {
    std::string text;
    std::string tooltip_text;
    std::string icon_name;
    std::string icon_category;
    mutable Icon cached_icon;
    mutable int cached_size = -1;
};

class StandardIconModel : public ItemModel {
  public:
    explicit StandardIconModel(std::vector<StandardIconItem> items = {});

    size_t row_count() const override { return items_.size(); }
    std::string cell_text(size_t row, size_t col) const override;
    std::string tooltip(size_t row) const override;
    Icon icon_at(size_t row, size_t col, int size) const override;

    void set_items(std::vector<StandardIconItem> items);
    void append(StandardIconItem item);

  private:
    std::vector<StandardIconItem> items_;
};

class FilterAdapter : public ItemModel, public std::enable_shared_from_this<FilterAdapter> {
  public:
    explicit FilterAdapter(std::shared_ptr<ItemModel> source);
    ~FilterAdapter() override;

    size_t row_count() const override { return indices_.size(); }
    size_t column_count() const override { return source_ ? source_->column_count() : 0; }
    std::string cell_text(size_t row, size_t col) const override;
    std::string header_text(size_t col) const override;
    Icon icon_at(size_t row, size_t col, int size) const override;
    std::string tooltip(size_t row) const override;

    void set_filter(std::string const &filter);
    std::string const &filter() const { return filter_; }
    std::optional<size_t> source_index(size_t filtered_index) const;

    void set_simulated_delay_ms(int ms) { delay_per_item_ms_ = ms; }
    std::function<void(float progress)> on_progress;

  private:
    void rebuild_sync();
    void rebuild_async();

    std::shared_ptr<ItemModel> source_;
    std::string filter_;
    std::vector<size_t> indices_;
    std::shared_ptr<std::atomic<int>> generation_ = std::make_shared<std::atomic<int>>(0);
    int delay_per_item_ms_ = 0;
};

// A model whose rows are represented by widgets rather than plain text.
// Maintains a prefix-sum cache of row Y offsets so lookups are O(1) and
// hit-testing is O(log n). Subclasses call append_offset() on each add (O(1))
// or rebuild_offsets() when doing bulk changes (O(n)).
// Use WidgetListModel (list_view.hpp) for the common ownership pattern.
class WidgetItemModel : public ItemModel {
  public:
    virtual Widget *widget_at(size_t row) = 0;
    std::string cell_text(size_t, size_t) const override { return {}; }

    // Top Y of row i in content space (O(1)).
    float row_top(size_t row) const {
        return row < row_tops_.size() ? row_tops_[row] : total_height_;
    }
    float total_height() const { return total_height_; }
    // Raw prefix-sum array — used by ListView for binary search in item_at_y().
    std::vector<float> const &row_tops() const { return row_tops_; }

  protected:
    // Full O(n) rebuild — call after bulk changes. Defined in item_model.cpp
    // because it calls widget_at() which needs the full Widget type.
    void rebuild_offsets();

    // O(1) incremental append — call before inserting one item at the end.
    void append_offset(float height) {
        row_tops_.push_back(total_height_);
        total_height_ += height;
    }

  private:
    std::vector<float> row_tops_;
    float total_height_ = 0.0f;
};

} // namespace toolkit
