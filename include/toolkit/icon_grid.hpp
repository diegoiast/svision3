// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image_loader.hpp"
#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace toolkit {

class IconGridModel {
  public:
    virtual ~IconGridModel() = default;
    virtual size_t count() const = 0;
    virtual std::string text_at(size_t index) const = 0;
    virtual std::string tooltip_at(size_t index) const { return text_at(index); }
    virtual Icon icon_at(size_t index, int size, bool snap) const = 0;

    std::function<void()> on_data_changed;
};

class SimpleIconGridModel : public IconGridModel {
  public:
    struct Item {
        std::string text;
        std::string tooltip;
        Icon icon;
    };

    explicit SimpleIconGridModel(std::vector<Item> items = {});

    size_t count() const override { return items_.size(); }
    std::string text_at(size_t index) const override;
    std::string tooltip_at(size_t index) const override;
    Icon icon_at(size_t index, int size, bool snap) const override;

    void set_items(std::vector<Item> items);
    void append(Item item);

  private:
    std::vector<Item> items_;
};

struct StandardIconItem {
    std::string text;
    std::string tooltip;
    std::string icon_name;
    std::string icon_category;
    mutable Icon cached_icon;
    mutable int cached_size = -1;
};

class StandardIconModel : public IconGridModel {
  public:
    explicit StandardIconModel(std::vector<StandardIconItem> items = {});

    size_t count() const override { return items_.size(); }
    std::string text_at(size_t index) const override;
    std::string tooltip_at(size_t index) const override;
    Icon icon_at(size_t index, int size, bool snap) const override;

    void set_items(std::vector<StandardIconItem> items);
    void append(StandardIconItem item);

  private:
    std::vector<StandardIconItem> items_;
};

class IconGrid : public Widget {
  public:
    explicit IconGrid(std::shared_ptr<IconGridModel> model);

    IconGrid &set_model(std::shared_ptr<IconGridModel> model);
    std::shared_ptr<IconGridModel> model() const { return model_; }

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

    int display_icon_size() const;
    LayoutInfo compute_layout() const;
    std::optional<size_t> item_at(Point p) const;
    size_t first_visible_item() const;
    void clamp_scroll();
    void scroll_to(size_t index);

    std::shared_ptr<IconGridModel> model_;
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
