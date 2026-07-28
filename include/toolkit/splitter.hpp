// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <cstdint>
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

    // Insert a child widget at index, shifting later children (and their
    // dividers) up by one. index is clamped to [0, child_count()], so
    // insert_child(child_count(), w) is equivalent to add_child(w). Like
    // add_child(), the new divider gets a default equal-distribution ratio;
    // call set_ratio() afterwards for exact positions.
    Splitter &insert_child(size_t index, std::unique_ptr<Widget> w);

    size_t child_count() const { return children_.size(); }
    WeakRefWidget<Widget> child_at(size_t index) const;

    // Remove and destroy the child at index, along with the divider adjacent to it.
    // Any in-progress drag/hover on a divider is cleared, since divider indices
    // shift after removal.
    Splitter &remove_child(size_t index);

    // Set/get the ratio (0..1) for divider i. Ratio is the centre of the handle
    // as a fraction of the splitter's total extent.
    Splitter &set_ratio(int divider, float r);
    float ratio(int divider) const;

    // Lock/unlock a single divider (hides its handle and prevents dragging).
    Splitter &set_divider_locked(int divider, bool locked);
    bool is_divider_locked(int divider) const;

    // How much of a splitter resize (not user dragging, which always stays
    // local to the dragged divider) child i absorbs, relative to the other
    // children. Every child defaults to 1, so by default all children
    // grow/shrink equally when the splitter is resized. A factor of 0 means
    // the child keeps its pixel size on resize; it only changes via dragging
    // or set_ratio().
    Splitter &set_stretch_factor(int child, float factor);
    float stretch_factor(int child) const;

    // Single-divider convenience, for the common 2-child splitter case.
    Splitter &set_ratio(float r) { return set_ratio(0, r); }
    float ratio() const { return ratio(0); }
    Splitter &set_locked(bool locked);  // lock/unlock all dividers
    bool locked() const;                // true if any divider is locked

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
    std::vector<float> ratios_;                        // child_count()-1 values
    mutable std::vector<DividerLock> locked_dividers_; // child_count()-1 entries
    std::vector<float> stretch_factors_;               // child_count() values
    float last_total_ = -1.0f; // total extent at the last real layout; -1 = none yet
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
    // Redistributes the (new_total - last_total_) delta across children by
    // stretch factor, rewriting ratios_ so the rest of the layout logic below
    // sees it as if the user had repositioned every divider explicitly.
    void redistribute_stretch(float new_total);
};

} // namespace toolkit
