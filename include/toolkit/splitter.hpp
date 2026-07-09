// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace toolkit {

class Splitter : public Widget, public Fluent<Splitter> {
    DECLARE_WIDGET(Splitter)
  public:
    explicit Splitter(Orientation o = Orientation::Horizontal);

    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    // Add a child widget to the end. A new divider is created between the previous
    // last child and this one with a default equal-distribution ratio. Call
    // set_ratio() after adding all children to set exact positions.
    Splitter &add_child(std::unique_ptr<Widget> w);
    size_t child_count() const { return children_.size(); }
    Widget *child_at(size_t index);

    // Set/get the ratio (0..1) for divider i. Ratio is the centre of the handle
    // as a fraction of the splitter's total extent.
    Splitter &set_ratio(int divider, float r);
    float ratio(int divider) const;

    // Lock/unlock a single divider (hides its handle and prevents dragging).
    Splitter &set_divider_locked(int divider, bool locked);
    bool is_divider_locked(int divider) const;

    // Backward-compat wrappers for existing 2-child code -----------------------
    Splitter &set_first(std::unique_ptr<Widget> w);
    Splitter &set_second(std::unique_ptr<Widget> w);
    Splitter &set_ratio(float r) { return set_ratio(0, r); }
    float ratio() const { return ratio(0); }
    Splitter &set_locked(bool locked);  // lock/unlock all dividers
    bool locked() const;                // true if any divider is locked
    // --------------------------------------------------------------------------

    Orientation orientation() const { return orientation_; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    CursorShape cursor() const override { return cursor_; }
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Size size_hint() const override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;
    void on_theme_changed() override;

  private:
    // A locked divider is hidden and not draggable. Its position is anchored in
    // pixels to the nearest edge (captured on the first layout after locking),
    // so a collapsed pane keeps its exact size when the splitter is resized —
    // a stored ratio would drift and leave a gap.
    struct DividerLock {
        uint8_t locked = 0;
        uint8_t from_end = 0; // anchor edge: 0 = start (left/top), 1 = end
        float px = std::numeric_limits<float>::quiet_NaN(); // NaN = not captured yet
    };

    std::vector<std::unique_ptr<Widget>> children_;
    std::vector<float> ratios_;                 // child_count()-1 values
    mutable std::vector<DividerLock> locked_dividers_; // child_count()-1 entries
    Orientation orientation_;
    CursorShape cursor_ = CursorShape::Arrow;
    std::optional<int> dragging_divider_;
    std::optional<int> hovered_divider_;

    static constexpr float kBorderWidth = 5.0f;
    static constexpr float kHitRadius = 2.0f;

    float effective_thickness(int divider) const;
    std::vector<float> compute_positions() const;
    Rect handle_rect(int divider, std::vector<float> const &positions) const;
    void layout_children();
};

} // namespace toolkit
