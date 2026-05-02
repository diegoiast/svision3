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

class IconGrid : public Widget {
  public:
    explicit IconGrid(std::shared_ptr<ItemModel> model);

    IconGrid &set_model(std::shared_ptr<ItemModel> model);
    std::shared_ptr<ItemModel> model() const { return model_; }

    IconGrid &set_icon_size(int size);
    int icon_size() const { return icon_size_; }

    IconGrid &set_scale_icons(bool scale);
    bool scale_icons() const { return scale_icons_; }

    std::optional<size_t> selected_index() const { return cursor_; }
    std::set<size_t> selected_indices() const { return selected_indices_; }
    IconGrid &set_selected(std::optional<size_t> index);
    IconGrid &toggle_selection(size_t index);
    IconGrid &select_range(size_t from, size_t to);
    IconGrid &select_in_rect(Rect const &r);
    Rect rubber_selection_rect() const;
    int display_icon_size() const;

    std::function<void(std::set<size_t> const &indices)> on_selection_changed;
    std::function<void(size_t index)> on_item_activated;
    std::function<void()> on_back_requested;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    void set_rect(Rect const &rect) override;
    Size size_hint() const override;
    Widget *widget_at(Point p) override;

  private:
    struct LayoutInfo {
        size_t columns;
        size_t rows;
        float item_width;
        float item_height;
    };

    LayoutInfo compute_layout() const;
    std::optional<size_t> item_at(Point p) const;
    size_t first_visible_item() const;
    void clamp_scroll();
    void scroll_to(size_t index);

    std::shared_ptr<ItemModel> model_;
    int icon_size_ = 48;
    bool scale_icons_ = false;
    std::optional<size_t> cursor_;
    std::set<size_t> selected_indices_;
    std::optional<size_t> hovered_;
    std::optional<size_t> selection_anchor_;
    float scroll_offset_ = 0;
    bool rubber_selecting_ = false;
    bool rubber_add_ = false;
    Point rubber_start_;
    Point rubber_end_;
};

} // namespace toolkit
