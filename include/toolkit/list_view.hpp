// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace toolkit {

class ListAdapter {
  public:
    virtual ~ListAdapter() = default;
    virtual int count() const = 0;
    // FIXME: convert to size_t
    virtual std::string text_at(int index) const = 0;

    std::function<void()> on_data_changed;
};

class StringListAdapter : public ListAdapter {
  public:
    explicit StringListAdapter(std::vector<std::string> items = {});

    int count() const override { return static_cast<int>(items_.size()); }
    std::string text_at(int index) const override;

    void set_items(std::vector<std::string> items);
    void append(std::string item);
    void remove(int index);

  private:
    std::vector<std::string> items_;
};

// FIXME: add a comperator pluggable function
class FilterAdapter : public ListAdapter, public std::enable_shared_from_this<FilterAdapter> {
  public:
    explicit FilterAdapter(std::shared_ptr<ListAdapter> source);
    ~FilterAdapter() override;

    int count() const override { return static_cast<int>(indices_.size()); }
    std::string text_at(int index) const override;

    void set_filter(std::string const &filter);
    std::string const &filter() const { return filter_; }

    // FIXME: convert to std::optional<size_t>
    int source_index(int filtered_index) const;

    // FIXME: remove this simulated delay
    void set_simulated_delay_ms(int ms) { delay_per_item_ms_ = ms; }

    std::function<void(float progress)> on_progress;

  private:
    // FIXME: remove sync rebuild. Always build in another thread
    void rebuild_sync();
    // FIXME: rename to rebuild
    void rebuild_async();

    std::shared_ptr<ListAdapter> source_;
    std::string filter_;
    std::vector<int> indices_;
    std::shared_ptr<std::atomic<int>> generation_ = std::make_shared<std::atomic<int>>(0);
    int delay_per_item_ms_ = 0;
};

class ListView : public Widget {
  public:
    explicit ListView(std::shared_ptr<ListAdapter> adapter);

    ListView &set_adapter(std::shared_ptr<ListAdapter> adapter);
    std::shared_ptr<ListAdapter> get_adapter() const { return adapter_; }

    ListView &set_alternating_row_colors(bool enabled) {
        alternating_ = enabled;
        return *this;
    }
    bool alternating_row_colors() const { return alternating_; }

    ListView &set_multi_select(bool enabled) {
        multi_select_ = enabled;
        return *this;
    }
    bool get_multi_select() const { return multi_select_; }

    int get_selected_index() const { return cursor_; }
    std::set<int> const &get_selection() const { return selection_; }
    ListView &set_selected(int index);
    ListView &set_selection(std::set<int> indices);
    ListView &select_all();
    ListView &clear_selection();
    bool is_selected(int index) const { return selection_.count(index) > 0; }

    std::function<void(int index)> on_selection_changed;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;

  private:
    float item_height() const;
    float total_content_height() const;
    void clamp_scroll();
    // FIXME: use std::optional<size_t>
    int item_at_y(float y) const;
    void scroll_to(int index);
    void select_range_from_anchor();
    void notify_selection();

    std::shared_ptr<ListAdapter> adapter_;
    std::set<int> selection_;
    // FIXME: use std::optional<size_t>
    int anchor_ = -1;
    // FIXME: use std::optional<size_t>
    int cursor_ = -1;
    // FIXME: use std::optional<size_t>
    int hovered_ = -1;
    float scroll_offset_ = 0;

    bool alternating_ = false;
    bool multi_select_ = false;
};

} // namespace toolkit
