// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace toolkit {

class TableModel {
  public:
    virtual ~TableModel() = default;

    virtual int row_count() const = 0;
    virtual int column_count() const = 0;
    virtual std::string_view header_text(int column) const = 0;
    virtual std::string_view cell_text(int row, int column) const = 0;

    std::function<void()> on_data_changed;
};

class StringTableModel : public TableModel {
  public:
    explicit StringTableModel(std::vector<std::string> headers,
                              std::vector<std::vector<std::string>> rows = {});

    int row_count() const override;
    int column_count() const override;
    std::string_view header_text(int column) const override;
    std::string_view cell_text(int row, int column) const override;

    void set_data(std::vector<std::string> headers, std::vector<std::vector<std::string>> rows);
    void append_row(std::vector<std::string> row);
    void remove_row(int index);

  private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
};

enum class SortOrder { None, Ascending, Descending };

class TableView : public Widget, public Fluent<TableView> {
  public:
    explicit TableView(std::shared_ptr<TableModel> model);

    TableView &set_model(std::shared_ptr<TableModel> model);
    std::shared_ptr<TableModel> model() const { return model_; }

    // Selection
    int selected_row() const { return cursor_row_; }
    std::set<int> const &selection() const { return selection_; }
    TableView &set_selected_row(int row);
    TableView &set_selection(std::set<int> rows);
    TableView &select_all();
    TableView &clear_selection();
    bool is_selected(int row) const { return selection_.count(row) > 0; }

    bool multi_select() const { return multi_select_; }
    TableView &set_multi_select(bool enabled);

    bool alternating_row_colors() const { return alternating_; }
    TableView &set_alternating_row_colors(bool enabled);

    // Column sizing
    TableView &set_column_width(int column, float width);
    float column_width(int column) const;

    // Sorting
    int sort_column() const { return sort_column_; }
    SortOrder sort_order() const { return sort_order_; }

    std::function<void(int row)> on_selection_changed;
    std::function<void(int column, SortOrder order)> on_sort_requested;

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
    int row_at_y(float y) const;
    int column_at_x(float x) const;
    int header_resize_hit(float x, float y) const;
    void scroll_to_row(int row);
    void select_range_from_anchor();
    void notify_selection();
    void ensure_column_widths();

    void rebuild_sort_index();
    int model_row(int display_row) const;

    std::shared_ptr<TableModel> model_;
    std::vector<float> column_widths_;
    std::vector<int> sort_indices_;
    std::set<int> selection_;
    int anchor_row_ = -1;
    int cursor_row_ = -1;
    int hovered_row_ = -1;
    float scroll_y_ = 0;
    float scroll_x_ = 0;
    bool alternating_ = false;
    bool multi_select_ = false;

    int sort_column_ = -1;
    SortOrder sort_order_ = SortOrder::None;

    int resize_col_ = -1;
    float resize_start_x_ = 0;
    float resize_start_w_ = 0;
    bool over_resize_grip_ = false;
};

} // namespace toolkit
