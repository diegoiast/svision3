// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/layout.hpp"
#include "toolkit/splitter.hpp"
#include "toolkit/tab_widget.hpp"
#include <array>

namespace toolkit {

enum class DockPosition { Left = 0, Right = 1, Top = 2, Bottom = 3 };

// DockArea manages a center widget and up to four docked TabWidget panels
// (left, right, top, bottom). On first layout it builds a nested Splitter
// tree so every dock boundary is drag-resizable. All docks must be added
// before the DockArea is first given a non-zero rect (i.e., before the
// window is shown).
class DockArea : public AbstractLayout {
    DECLARE_WIDGET(DockArea)
  public:
    DockArea();

    DockArea &set_center(std::unique_ptr<Widget> widget);
    template <class T> T &set_center() {
        auto ptr = std::make_unique<T>();
        T &ref = *ptr;
        set_center(std::move(ptr));
        return ref;
    }

    // Add a panel to a docked side. Returns the TabWidget for that side so
    // callers can configure orientation, etc.
    TabWidget &add_dock(DockPosition pos, std::string title, std::unique_ptr<Widget> content);
    template <class T> T &add_dock(DockPosition pos, std::string_view title) {
        auto ptr = std::make_unique<T>();
        T &ref = *ptr;
        add_dock(pos, std::string(title), std::move(ptr));
        return ref;
    }

    // Initial size for a side before the splitter tree is built.
    DockArea &set_dock_size(DockPosition pos, float size);
    float dock_size(DockPosition pos) const { return sides_[int(pos)].size; }

    // Access the TabWidget for a side (valid before and after tree build).
    TabWidget *dock_tab_widget(DockPosition pos) const { return sides_[int(pos)].ptr; }

    void for_each_child(std::function<void(Widget *)> const &callback) override;
    Size size_hint() const override;

    static constexpr float default_dock_size = 220.0f;

  protected:
    void apply_layout() override;

  private:
    struct DockSide {
        std::unique_ptr<TabWidget> tabs; // owned until tree is built
        TabWidget *ptr = nullptr;        // always valid once add_dock is called
        Splitter *splitter = nullptr;    // the Splitter that directly contains this dock's TabWidget
        float size = default_dock_size;
    };

    void build_splitter_tree();

    std::unique_ptr<Widget> center_;
    std::array<DockSide, 4> sides_;
    std::unique_ptr<Widget> root_; // Splitter tree root; null until first layout
};

} // namespace toolkit
