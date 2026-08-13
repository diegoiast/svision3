// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/dock_area.hpp"
#include "svision3/splitter.hpp"
#include "svision3/window.hpp"

#include <algorithm>

namespace svision3 {

DockArea::DockArea() {
    auto west = std::make_shared<TabWidget>();
    west->set_tabs_closable(false)
        .set_focus_on_tab_click(false)
        .set_orientation(TabOrientation::East)
        .set_collapsible(true)
        .set_tabs_movable(true);
    westDocks = west;

    auto east = std::make_shared<TabWidget>();
    east->set_tabs_closable(false)
        .set_focus_on_tab_click(false)
        .set_orientation(TabOrientation::West)
        .set_collapsible(true)
        .set_tabs_movable(true);
    eastDocks = east;

    auto south = std::make_shared<TabWidget>();
    south->set_tabs_closable(false)
        .set_focus_on_tab_click(false)
        .set_orientation(TabOrientation::South)
        .set_collapsible(true)
        .set_tabs_movable(true);
    southDocks = south;

    auto vsplit = std::make_shared<Splitter>(Orientation::Vertical);
    vsplit->add_child(std::move(south));
    vsplit->set_stretch_factor(0, 0.0f);
    verticalSplitter = vsplit;

    // add_child() returns a weak_ptr now rather than *this, so these are three
    // statements instead of a chain. Order still matters: east=0, vsplit=1, west=2.
    add_child(std::move(east));
    add_child(vsplit);
    add_child(std::move(west));
    set_stretch_factor(0, 0.0f);
    set_stretch_factor(2, 0.0f);

    // host stays a raw Splitter*: for east/west it is `this`, which has no
    // shared_ptr to hand out from inside the constructor, and every host
    // outlives the slots either way.
    dock_slots_[int(DockPosition::East)] = {eastDocks, this, 0, true};
    dock_slots_[int(DockPosition::West)] = {westDocks, this, 1, false};
    dock_slots_[int(DockPosition::South)] = {southDocks, vsplit.get(), 0, false};

    for (auto pos : {DockPosition::East, DockPosition::West, DockPosition::South}) {
        if (auto tab = dock_slots_[int(pos)].tab.lock()) {
            tab->on_collapsed = [this, pos](bool collapsed) { apply_dock_size(pos, collapsed); };
        }
    }
}

void DockArea::set_rect(Rect const &rect) {
    Splitter::set_rect(rect);

    if (!sizes_applied_ && rect.width > 0.0f && rect.height > 0.0f) {
        sizes_applied_ = true;
        for (auto pos : {DockPosition::East, DockPosition::West, DockPosition::South}) {
            auto tab = dock_slots_[int(pos)].tab.lock();
            apply_dock_size(pos, tab && tab->is_collapsed());
        }
    }
}

void DockArea::apply_dock_size(DockPosition pos, bool collapsed) {
    auto const &slot = dock_slots_[int(pos)];
    auto tab = slot.tab.lock();
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

std::weak_ptr<Widget> DockArea::set_center(std::shared_ptr<Widget> widget) {
    auto vs = verticalSplitter.lock();
    if (!vs) {
        return {};
    }
    if (vs->child_count() == 2) {
        vs->remove_child(0);
    }

    central = vs->insert_child(0, std::move(widget));
    return central;
}

std::weak_ptr<Widget> DockArea::add_dock(DockPosition pos, const std::string &title,
                                         std::shared_ptr<Widget> content) {
    auto tab_ref = std::weak_ptr<TabWidget>{};

    switch (pos) {
    case DockPosition::West:
        tab_ref = westDocks;
        break;
    case DockPosition::South:
        tab_ref = southDocks;
        break;
    case DockPosition::East:
        tab_ref = eastDocks;
        break;
    }

    auto tab = tab_ref.lock();
    if (!tab) {
        return {};
    }

    auto was_empty = tab->get_tab_count() == 0;
    auto result = tab->add_tab(title, std::move(content), false);
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

    auto tab = dock_slots_[int(pos)].tab.lock();
    if (tab && !tab->is_collapsed()) {
        apply_dock_size(pos, false);
    }
    return *this;
}

float DockArea::dock_size(DockPosition pos) const { return dock_sizes_[int(pos)]; }

std::weak_ptr<TabWidget> DockArea::dock_tab_widget(DockPosition pos) const {
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

} // namespace svision3
