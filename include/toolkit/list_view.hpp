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

// Concrete WidgetItemModel that owns its widgets.
// add()       — O(1) incremental offset update, then notifies.
// set_items() — O(n) full rebuild, then notifies.
class WidgetListModel : public WidgetItemModel {
  public:
    size_t row_count() const override { return items_.size(); }
    Widget *widget_at(size_t row) override {
        return row < items_.size() ? items_[row].get() : nullptr;
    }

    WidgetListModel &add(std::unique_ptr<Widget> w) {
        append_offset(w->size_hint().height);
        items_.push_back(std::move(w));
        if (on_data_changed) {
            on_data_changed();
        }
        return *this;
    }

    WidgetListModel &set_items(std::vector<std::unique_ptr<Widget>> widgets) {
        items_ = std::move(widgets);
        rebuild_offsets();
        if (on_data_changed) {
            on_data_changed();
        }
        return *this;
    }

    Widget *at(size_t i) { return i < items_.size() ? items_[i].get() : nullptr; }

  private:
    std::vector<std::unique_ptr<Widget>> items_;
};

class ListView : public Widget {
  public:
    explicit ListView(std::shared_ptr<ItemModel> model);

    ListView &set_model(std::shared_ptr<ItemModel> model);
    std::shared_ptr<ItemModel> model() const { return model_; }

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

    std::optional<size_t> selected_index() const { return cursor_; }
    std::set<size_t> const &selection() const { return selection_; }
    ListView &set_selected(std::optional<size_t> index);
    ListView &set_selection(std::set<size_t> indices);
    ListView &select_all();
    ListView &clear_selection();
    bool is_selected(size_t index) const { return selection_.count(index) > 0; }

    std::function<void(std::optional<size_t> index)> on_selection_changed;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    void set_window(Window *w) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

  private:
    float item_height() const;
    float total_content_height() const;
    void clamp_scroll();
    std::optional<size_t> item_at_y(float y) const;
    void scroll_to(size_t index);
    void select_range_from_anchor();
    void notify_selection();

    // Widget-model support
    void sync_widget_windows();
    bool dispatch_to_widget(size_t row, MouseEvent event);
    void paint_text_items(Painter &painter);
    void paint_widget_items(Painter &painter, WidgetItemModel *wm);

    std::shared_ptr<ItemModel> model_;
    std::set<size_t> selection_;
    std::optional<size_t> anchor_;
    std::optional<size_t> cursor_;
    std::optional<size_t> hovered_;
    std::optional<size_t> pressed_widget_row_;
    float scroll_offset_ = 0;

    bool alternating_ = false;
    bool multi_select_ = false;
};

} // namespace toolkit
