// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

// FIXME: this needs to move to a new header, theme forwards it - and it looks weird.
enum class TabOrientation { North, South, East, West };

class TabWidget : public Widget {
  public:
    TabWidget();

    void add_tab(std::string title, std::unique_ptr<Widget> content);

    int current_index() const { return current_; }
    void set_current(int index);

    TabOrientation orientation() const { return orientation_; }
    void set_orientation(TabOrientation o);

    void set_leading_widget(std::unique_ptr<Widget> widget);
    void set_trailing_widget(std::unique_ptr<Widget> widget);

    std::function<void(int index, std::string const &title)> on_tab_close;

    void scroll_to_tab(int index);
    void scroll_by(float delta);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

  private:
    struct Tab {
        std::string title;
        std::unique_ptr<Widget> content;
    };

    static constexpr float close_btn_size_ = 14.0f;
    static constexpr float close_btn_gap_ = 6.0f;

    float tab_bar_thickness() const;
    float tab_size(int i) const;
    void layout_content();
    void update_scroll_bounds();
    bool handle_tab_drag(MouseEvent const &event);

    struct HitResult {
        int tab = -1;
        bool on_close = false;
    };
    HitResult hit_test_tab(Point p) const;

    std::vector<Tab> tabs_;
    TabOrientation orientation_ = TabOrientation::North;
    int current_ = 0;
    int hovered_tab_ = -1;
    int hovered_close_ = -1;

    std::unique_ptr<Widget> leading_widget_;
    std::unique_ptr<Widget> trailing_widget_;

    std::unique_ptr<class Button> prev_button_;
    std::unique_ptr<class Button> next_button_;

    bool show_scroll_buttons_ = false;
    float scroll_offset_ = 0;

    bool dragging_ = false;
    int drag_tab_ = -1;
    float drag_start_x_ = 0;
    float drag_offset_x_ = 0;
};

} // namespace toolkit
