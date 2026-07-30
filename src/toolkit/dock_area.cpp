// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/dock_area.hpp"
#include "toolkit/splitter.hpp"
#include "toolkit/window.hpp"

#include <algorithm>

namespace toolkit {

DockArea::DockArea() {
    auto west = std::make_unique<TabWidget>();
    west->set_tabs_closable(false)
        .set_focus_on_tab_click(false)
        .set_orientation(TabOrientation::East)
        .set_collapsible(true)
        .set_tabs_movable(true);
    westDocks = weak_ref(west.get());

    auto east = std::make_unique<TabWidget>();
    east->set_tabs_closable(false)
        .set_focus_on_tab_click(false)
        .set_orientation(TabOrientation::West)
        .set_collapsible(true)
        .set_tabs_movable(true);
    eastDocks = weak_ref(east.get());

    auto south = std::make_unique<TabWidget>();
    south->set_tabs_closable(false)
        .set_focus_on_tab_click(false)
        .set_orientation(TabOrientation::South)
        .set_collapsible(true)
        .set_tabs_movable(true);
    southDocks = weak_ref(south.get());

    auto vsplit = std::make_unique<Splitter>(Orientation::Vertical);
    vsplit->add_child(std::move(south));
    vsplit->set_stretch_factor(0, 0.0f);
    verticalSplitter = weak_ref(vsplit.get());

    add_child(std::move(east)).add_child(std::move(vsplit)).add_child(std::move(west));
    set_stretch_factor(0, 0.0f);
    set_stretch_factor(2, 0.0f);

    dock_slots_[int(DockPosition::East)] = {eastDocks, this, 0, true};
    dock_slots_[int(DockPosition::West)] = {westDocks, this, 1, false};
    dock_slots_[int(DockPosition::South)] = {southDocks, verticalSplitter.get(), 0, false};

    for (auto pos : {DockPosition::East, DockPosition::West, DockPosition::South}) {
        auto *tab = dock_slots_[int(pos)].tab.get();
        tab->on_collapsed = [this, pos](bool collapsed) { apply_dock_size(pos, collapsed); };
    }
}

void DockArea::set_rect(Rect const &rect) {
    Splitter::set_rect(rect);

    if (!sizes_applied_ && rect.width > 0.0f && rect.height > 0.0f) {
        sizes_applied_ = true;
        for (auto pos : {DockPosition::East, DockPosition::West, DockPosition::South}) {
            auto *tab = dock_slots_[int(pos)].tab.get();
            apply_dock_size(pos, tab && tab->is_collapsed());
        }
    }
}

void DockArea::apply_dock_size(DockPosition pos, bool collapsed) {
    auto const &slot = dock_slots_[int(pos)];
    auto *tab = slot.tab.get();
    auto *host = slot.host;

    if (!tab || !host) {
        return;
    }

    auto r = host->rect();
    auto total = (host->orientation() == Orientation::Horizontal) ? r.width : r.height;

    if (total <= 0.0f) {
        return;
    }
    // An empty dock (no tabs added yet) takes no space at all, regardless of
    // its own collapsed state -- collapsing to a tab bar only makes sense
    // once there's actually a tab bar to show.
    auto hidden = tab->get_tab_count() == 0;
    auto size = hidden ? 0.0f : (collapsed ? tab->tab_bar_size() : dock_sizes_[int(pos)]);
    auto ratio = slot.near_start ? size / total : 1.0f - size / total;
    host->set_ratio(slot.divider, ratio);
    host->set_divider_locked(slot.divider, hidden || collapsed);
}

Widget &DockArea::set_center(std::unique_ptr<Widget> widget) {
    auto *vs = verticalSplitter.get();
    if (vs->child_count() == 2) {
        vs->remove_child(0);
    }

    auto *widget_ptr = widget.get();
    vs->insert_child(0, std::move(widget));
    central = weak_ref(widget_ptr);
    return *widget_ptr;
}

Widget &DockArea::add_dock(DockPosition pos, const std::string &title,
                           std::unique_ptr<Widget> content) {
    auto tab = WeakRefWidget<TabWidget>{};

    switch (pos) {
    case DockPosition::West:
        tab = westDocks;
        break;
    case DockPosition::South:
        tab = southDocks;
        break;
    case DockPosition::East:
        tab = eastDocks;
        break;
    }

    auto was_empty = tab->get_tab_count() == 0;
    auto &result = tab->add_tab(title, std::move(content), false);
    if (was_empty) {
        // Reveal the dock now that it actually has something to show. This
        // goes through apply_dock_size() directly rather than set_collapsed(),
        // since the dock was never "collapsed" in the TabWidget's own sense --
        // it was hidden by the divider lock while empty, independent of
        // TabWidget::collapsed_.
        apply_dock_size(pos, false);
    }
    return result;
}

DockArea &DockArea::set_dock_size(DockPosition pos, float size) {
    dock_sizes_[int(pos)] = std::max(50.0f, size);

    auto *tab = dock_slots_[int(pos)].tab.get();
    if (tab && !tab->is_collapsed()) {
        apply_dock_size(pos, false);
    }
    return *this;
}

float DockArea::dock_size(DockPosition pos) const { return dock_sizes_[int(pos)]; }

WeakRefWidget<TabWidget> DockArea::dock_tab_widget(DockPosition pos) const {
    switch (pos) {
    case DockPosition::West:
        return westDocks;
    case DockPosition::South:
        return southDocks;
    case DockPosition::East:
        return eastDocks;
    }

    return {};
}

} // namespace toolkit
