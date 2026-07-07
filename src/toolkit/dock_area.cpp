// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/dock_area.hpp"
#include "toolkit/splitter.hpp"
#include "toolkit/window.hpp"

#include <algorithm>

namespace toolkit {

DockArea::DockArea() {}

DockArea &DockArea::set_center(std::unique_ptr<Widget> widget) {
    center_ = std::move(widget);
    if (center_) {
        center_->set_parent(this);
        if (window_) {
            center_->set_window(window_);
        }
    }
    return *this;
}

TabWidget &DockArea::add_dock(DockPosition pos, std::string title,
                              std::unique_ptr<Widget> content) {
    auto &side = sides_[int(pos)];
    if (!side.tabs) {
        side.tabs = std::make_unique<TabWidget>();
        side.ptr = side.tabs.get();
        side.tabs->set_parent(this);
        if (window_) {
            side.tabs->set_window(window_);
        }
        side.tabs->set_tabs_closable(false);
        side.tabs->set_focus_on_tab_click(false);
    }
    side.ptr->add_tab(std::move(title), std::move(content), false);
    return *side.ptr;
}

DockArea &DockArea::set_dock_size(DockPosition pos, float size) {
    sides_[int(pos)].size = std::max(50.0f, size);
    return *this;
}

void DockArea::for_each_child(std::function<void(Widget *)> const &callback) {
    if (root_) {
        callback(root_.get());
    } else {
        for (auto &side : sides_) {
            if (side.tabs) {
                callback(side.tabs.get());
            }
        }
        if (center_) {
            callback(center_.get());
        }
    }
}

void DockArea::build_splitter_tree() {
    // Build a nested Splitter tree from the active docks. All docks are assumed
    // to have been added before first layout. After this call the TabWidgets are
    // owned by the Splitter tree; sides_[i].tabs is null but sides_[i].ptr stays valid.
    //
    // Build order: top/bottom around center first, then left/right around that
    // column. This makes the left and right sidebars span the full window height
    // (including over the top/bottom dock areas).

    auto content_w = rect_.width - margins_.left - margins_.right;
    auto content_h = rect_.height - margins_.top - margins_.bottom;

    std::unique_ptr<Widget> current = std::move(center_);

    auto wire_collapse = [](DockSide &side, Splitter *spl, bool first_slot) {
        side.splitter = spl;
        side.ptr->set_collapsible(true);
        side.ptr->on_collapsed = [&side, first_slot](bool collapsed) {
            if (!side.splitter) {
                return;
            }
            auto *spl = side.splitter;
            auto r = spl->rect();
            auto total = (spl->orientation() == Orientation::Horizontal) ? r.width : r.height;
            if (total <= 0) {
                return;
            }
            if (collapsed) {
                auto bar = side.ptr->tab_bar_size();
                spl->set_ratio(first_slot ? bar / total : 1.0f - bar / total);
                spl->set_locked(true);
            } else {
                spl->set_ratio(first_slot ? side.size / total : 1.0f - side.size / total);
                spl->set_locked(false);
            }
        };
    };

    // Top: vertical splitter  [top | current]
    auto &top = sides_[int(DockPosition::Top)];
    if (top.tabs) {
        auto spl = std::make_unique<Splitter>(Orientation::Vertical);
        spl->set_ratio(content_h > 0 ? top.size / content_h : 0.25f);
        wire_collapse(top, spl.get(), true);
        spl->set_first(std::move(top.tabs));
        spl->set_second(std::move(current));
        current = std::move(spl);
    }

    // Bottom: vertical splitter  [current | bottom]
    auto &bot = sides_[int(DockPosition::Bottom)];
    if (bot.tabs) {
        auto spl = std::make_unique<Splitter>(Orientation::Vertical);
        spl->set_ratio(content_h > 0 ? 1.0f - bot.size / content_h : 0.75f);
        wire_collapse(bot, spl.get(), false);
        spl->set_first(std::move(current));
        spl->set_second(std::move(bot.tabs));
        current = std::move(spl);
    }

    // Left: horizontal splitter  [left | current]
    auto &lft = sides_[int(DockPosition::Left)];
    if (lft.tabs) {
        auto spl = std::make_unique<Splitter>(Orientation::Horizontal);
        spl->set_ratio(content_w > 0 ? lft.size / content_w : 0.25f);
        wire_collapse(lft, spl.get(), true);
        spl->set_first(std::move(lft.tabs));
        spl->set_second(std::move(current));
        current = std::move(spl);
    }

    // Right: horizontal splitter  [current | right]
    auto &rgt = sides_[int(DockPosition::Right)];
    if (rgt.tabs) {
        auto spl = std::make_unique<Splitter>(Orientation::Horizontal);
        spl->set_ratio(content_w > 0 ? 1.0f - rgt.size / content_w : 0.75f);
        wire_collapse(rgt, spl.get(), false);
        spl->set_first(std::move(current));
        spl->set_second(std::move(rgt.tabs));
        current = std::move(spl);
    }

    root_ = std::move(current);
    if (root_) {
        root_->set_parent(this);
        if (window_) {
            root_->set_window(window_);
        }
    }
}

void DockArea::apply_layout() {
    if (!root_) {
        build_splitter_tree();
    }
    auto x = margins_.left;
    auto y = margins_.top;
    auto w = rect_.width - margins_.left - margins_.right;
    auto h = rect_.height - margins_.top - margins_.bottom;
    if (root_) {
        root_->set_rect({x, y, w, h});
    } else if (center_) {
        center_->set_rect({x, y, w, h});
    }
}

auto DockArea::size_hint() const -> Size {
    // Once the splitter tree is built it knows the real minimums (including
    // collapsed/shaded docks), so delegate to it directly.
    if (root_) {
        auto s = root_->size_hint();
        return {s.width + margins_.left + margins_.right,
                s.height + margins_.top + margins_.bottom};
    }
    // Pre-build fallback: sum up the requested dock sizes.
    auto w = center_ ? center_->size_hint().width : 200.0f;
    auto h = center_ ? center_->size_hint().height : 200.0f;
    if (sides_[int(DockPosition::Left)].ptr) {
        w += sides_[int(DockPosition::Left)].size;
    }
    if (sides_[int(DockPosition::Right)].ptr) {
        w += sides_[int(DockPosition::Right)].size;
    }
    if (sides_[int(DockPosition::Top)].ptr) {
        h += sides_[int(DockPosition::Top)].size;
    }
    if (sides_[int(DockPosition::Bottom)].ptr) {
        h += sides_[int(DockPosition::Bottom)].size;
    }
    return {w + margins_.left + margins_.right, h + margins_.top + margins_.bottom};
}

} // namespace toolkit
