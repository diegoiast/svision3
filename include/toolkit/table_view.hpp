// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/item_model.hpp"
#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace toolkit {

enum class SortOrder { None, Ascending, Descending };

class TableView : public Widget, public Fluent<TableView> {
  public:
    explicit TableView(std::shared_ptr<ItemModel> model);

    TableView &set_model(std::shared_ptr<ItemModel> model);
    std::shared_ptr<ItemModel> model() const { return model_; }

    std::optional<size_t> selected_row() const { return cursor_row_; }
    std::set<size_t> const &selection() const { return selection_; }
    TableView &set_selected_row(std::optional<size_t> row);
    TableView &set_selection(std::set<size_t> rows);
    TableView &select_all();
    TableView &clear_selection();
    bool is_selected(size_t row) const { return selection_.count(row) > 0; }

    bool multi_select() const { return multi_select_; }
    TableView &set_multi_select(bool enabled);

    bool alternating_row_colors() const { return alternating_; }
    TableView &set_alternating_row_colors(bool enabled);

    bool show_header() const { return show_header_; }
    TableView &set_show_header(bool show);

    TableView &set_column_width(int column, float width);
    float column_width(int column) const;

    int sort_column() const { return sort_column_; }
    SortOrder sort_order() const { return sort_order_; }

    std::function<void(std::optional<size_t> row)> on_selection_changed;
    std::function<void(int column, SortOrder order)> on_sort_requested;
    std::function<void(size_t row)> on_item_activated;
    std::function<void()> on_back_requested;

    void auto_fit_columns();
    void auto_fit_column(int column);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override;

  private:
    float row_height() const;
    float header_height() const;
    float total_content_height() const;
    float total_content_width() const;
    void clamp_scroll();
    std::optional<size_t> row_at_y(float y) const;
    int column_at_x(float x) const;
    int header_resize_hit(float x, float y) const;
    void scroll_to_row(size_t row);
    void select_range_from_anchor();
    void notify_selection();
    void ensure_column_widths();

    void rebuild_sort_index();
    size_t model_row(size_t display_row) const;

    std::shared_ptr<ItemModel> model_;
    std::vector<float> column_widths_;
    std::vector<size_t> sort_indices_;
    std::set<size_t> selection_;
    std::optional<size_t> anchor_row_;
    std::optional<size_t> cursor_row_;
    std::optional<size_t> hovered_row_;
    float scroll_y_ = 0;
    float scroll_x_ = 0;
    bool alternating_ = false;
    bool multi_select_ = false;
    bool show_header_ = true;

    int sort_column_ = -1;
    SortOrder sort_order_ = SortOrder::None;

    int resize_col_ = -1;
    float resize_start_x_ = 0;
    float resize_start_w_ = 0;
    bool over_resize_grip_ = false;
};

} // namespace toolkit
