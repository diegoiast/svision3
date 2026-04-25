// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/item_model.hpp"
#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <set>

namespace toolkit {

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

  private:
    float item_height() const;
    float total_content_height() const;
    void clamp_scroll();
    std::optional<size_t> item_at_y(float y) const;
    void scroll_to(size_t index);
    void select_range_from_anchor();
    void notify_selection();

    std::shared_ptr<ItemModel> model_;
    std::set<size_t> selection_;
    std::optional<size_t> anchor_;
    std::optional<size_t> cursor_;
    std::optional<size_t> hovered_;
    float scroll_offset_ = 0;

    bool alternating_ = false;
    bool multi_select_ = false;
};

} // namespace toolkit
