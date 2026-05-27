// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <string>
#include <vector>

namespace toolkit {

class Combobox : public Widget, public Fluent<Combobox> {
    DECLARE_WIDGET(Combobox)
  public:
    explicit Combobox(std::vector<std::string> items = {});
    nlohmann::json to_json() const override;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override { return CursorShape::Hand; }

    Combobox &set_items(std::vector<std::string> items);
    Combobox &set_selected(int index);
    int selected() const { return selected_index_; }
    std::string selected_text() const;

    std::function<void(int)> on_change;

  private:
    void open_dropdown();
    void close_dropdown();
    void paint_dropdown(Painter &painter);
    bool handle_dropdown_mouse(MouseEvent const &event);
    bool handle_dropdown_key(KeyEvent const &event);
    Rect dropdown_bounds() const;
    int item_index_at(Point p) const;

    float dropdown_item_height() const;
    void clamp_drop_scroll();
    void ensure_hovered_visible();

    std::vector<std::string> items_;
    int selected_index_ = -1;
    int hovered_index_ = -1;
    bool open_ = false;
    float drop_scroll_ = 0;
    int drop_max_visible_ = 0;
};

} // namespace toolkit
