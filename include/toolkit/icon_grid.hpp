// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include "toolkit/image_loader.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace toolkit {

class IconGridModel {
  public:
    virtual ~IconGridModel() = default;
    virtual size_t count() const = 0;
    virtual std::string text_at(size_t index) const = 0;
    virtual Icon icon_at(size_t index, int size, bool snap) const = 0;

    std::function<void()> on_data_changed;
};

class SimpleIconGridModel : public IconGridModel {
  public:
    struct Item {
        std::string text;
        std::string icon_name;
        Icon cached_icon;
        int cached_size = 0;
    };

    explicit SimpleIconGridModel(std::vector<Item> items = {});

    size_t count() const override { return items_.size(); }
    std::string text_at(size_t index) const override;
    Icon icon_at(size_t index, int size, bool snap) const override;

    void set_items(std::vector<Item> items);
    void append(Item item);

  private:
    std::vector<Item> items_;
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
    IconGrid &set_selected(std::optional<size_t> index);

    std::function<void(std::optional<size_t> index)> on_selection_changed;
    std::function<void(size_t index)> on_item_activated;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    void set_rect(Rect const &rect) override;
    Size size_hint() const override;

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
    std::optional<size_t> hovered_;
    float scroll_offset_ = 0;
};

} // namespace toolkit
