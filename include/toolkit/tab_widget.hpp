// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class TabWidget : public Widget, public Fluent<TabWidget> {
    DECLARE_WIDGET(TabWidget)
  public:
    TabWidget();
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    TabWidget &add_tab(std::string title, std::unique_ptr<Widget> content, bool closable = true);
    TabWidget &remove_tab(int index);

    int current_index() const { return current_; }
    TabWidget &set_current(int index);

    size_t tab_count() const { return tabs_.size(); }
    std::string tab_title(int index) const;
    TabWidget &set_tab_tooltip(int index, std::string tooltip);

    Widget *widget(int index) const;
    Widget *current_widget() const { return widget(current_); }

    TabWidget &set_tabs_closable(bool closable);
    bool tabs_closable() const { return tabs_closable_; }

    TabWidget &set_min_tab_width(float width);
    float min_tab_width() const { return min_tab_width_; }

    TabWidget &set_tabs_movable(bool movable) {
        tabs_movable_ = movable;
        return *this;
    }
    bool tabs_movable() const { return tabs_movable_; }

    // When false, clicking a tab header switches the tab but does not move
    // keyboard focus to this TabWidget. Useful for dock panels where the
    // center content should remain focused.
    TabWidget &set_focus_on_tab_click(bool f) {
        focus_on_tab_click_ = f;
        return *this;
    }
    bool focus_on_tab_click() const { return focus_on_tab_click_; }

    // Collapsed tab widget, show only the tab headers. They are used mainly
    // in dock areas.
    // When true, clicking the currently-active tab collapses the content area,
    // leaving only the tab bar visible. Clicking the same tab again expands it.
    TabWidget &set_collapsible(bool c) {
        collapsible_ = c;
        return *this;
    }

    bool is_collapsible() const { return collapsible_; }
    TabWidget &set_collapsed(bool c);
    bool is_collapsed() const { return collapsed_; }

    // Function to be called when set_collapsed is called
    std::function<void(bool collapsed)> on_collapsed;

    // Returns the pixel thickness of the tab bar strip (width for side bars,
    // height for top/bottom bars). Useful for computing collapse ratios.
    float tab_bar_size() const;

    TabWidget &set_orientation(TabOrientation o);
    TabOrientation orientation() const { return orientation_; }

    TabWidget &set_leading_widget(std::unique_ptr<Widget> widget);
    TabWidget &set_trailing_widget(std::unique_ptr<Widget> widget);

    std::function<void(int index, std::string const &title)> on_tab_close;

    // Fired whenever the active tab changes, whether via a click, keyboard
    // navigation, set_current(), or a tab close that shifts the current index.
    std::function<void(int index)> on_current_changed;

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
    void on_theme_changed() override;

  private:
    struct Tab {
        std::string title;
        std::string tooltip;
        bool closable = true;
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

    std::unique_ptr<class StackedLayout> content_layout_;
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
    bool tabs_closable_ = true;
    float min_tab_width_ = 0.0f;
    bool tabs_movable_ = true;
    bool focus_on_tab_click_ = true;
    bool collapsible_ = false;
    bool collapsed_ = false;

    bool dragging_ = false;
    int drag_tab_ = -1;
    float drag_start_x_ = 0;
    float drag_offset_x_ = 0;
    int pending_collapse_tab_ = -1; // tab pressed down for collapse; collapse fires on release
};

} // namespace toolkit
