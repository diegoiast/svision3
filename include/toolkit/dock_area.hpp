// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/layout.hpp"
#include "toolkit/splitter.hpp"
#include "toolkit/tab_widget.hpp"
#include <array>

namespace toolkit {

enum class DockPosition {
    West = 0,
    South = 1,
    East = 2,
};

// DockArea manages a center widget and up to four docked TabWidget panels
// (left, right, top, bottom). On first layout it builds a nested Splitter
// tree so every dock boundary is drag-resizable. All docks must be added
// before the DockArea is first given a non-zero rect (i.e., before the
// window is shown).
class DockArea : public Splitter {
    DECLARE_WIDGET(DockArea)
  public:
    DockArea();

    WeakRefWidget<Widget> get_center() const { return central; }
    Widget &set_center(std::unique_ptr<Widget> widget);
    Widget &add_dock(DockPosition pos, const std::string &title, std::unique_ptr<Widget> content);

    // The pixel size of a dock along its splitter's axis. Applied immediately
    // if the dock is currently visible; otherwise remembered for the next
    // time it's uncollapsed. This is a soft request, not a guarantee: like
    // any Splitter child, the dock's content minimum size can still force it
    // larger, or the window can force it smaller.
    DockArea &set_dock_size(DockPosition pos, float size);
    float dock_size(DockPosition pos) const;

    WeakRefWidget<TabWidget> dock_tab_widget(DockPosition pos) const;

    void set_rect(Rect const &rect) override;

    static constexpr float default_dock_size = 220.0f;

  private:
    // Where a dock's TabWidget lives: which Splitter it's a direct child of,
    // which divider of that splitter controls its size, and whether it sits
    // on the near (start) or far (end) side of that divider. Built once in
    // the constructor; shared by the collapse wiring and set_dock_size so
    // both agree on the same divider math.
    struct DockSlot {
        WeakRefWidget<TabWidget> tab;
        Splitter *host = nullptr;
        int divider = 0;
        bool near_start = false;
    };
    std::array<DockSlot, 3> dock_slots_;
    std::array<float, 3> dock_sizes_{default_dock_size, default_dock_size, default_dock_size};

    void apply_dock_size(DockPosition pos, bool collapsed);

    // Docks are usually configured via set_dock_size() before the window is
    // ever shown, i.e. before this DockArea has a real rect and set_ratio()
    // is a no-op. Applied once, the first time set_rect() sees a non-zero size.
    bool sizes_applied_ = false;

    WeakRefWidget<Splitter> verticalSplitter;
    WeakRefWidget<TabWidget> westDocks;
    WeakRefWidget<TabWidget> eastDocks;
    WeakRefWidget<TabWidget> southDocks;
    WeakRefWidget<Widget> central;
};

} // namespace toolkit
