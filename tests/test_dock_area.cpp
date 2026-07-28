#include "toolkit/button.hpp"
#include "toolkit/dock_area.hpp"
#include "toolkit/label.hpp"
#include "toolkit/window.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

// Regression test for a bug where a collapsed dock could never be
// un-collapsed by clicking it. Root cause: Window::handle_mouse resolves a
// Press to a "captured" widget via find_focusable_at before falling back to
// widget_at. TabWidget::find_focusable_at didn't check hit_test()/collapsed_
// the way widget_at did, so it reached into the collapsed dock's hidden
// content (grabbing a focusable widget there) instead of the tab bar. The
// click never reached TabWidget::handle_mouse, so collapse could never be
// toggled back off. This exercises the exact path a real click takes,
// through a real DockArea (whose Splitter shrinks a collapsed dock down to
// its tab bar's thickness -- the condition needed to trigger the bug).
TEST_CASE("Window-routed click on a collapsed dock's tab bar un-collapses it", "[dockarea]") {
    Window win("Test", {800, 600});

    auto dock = std::make_unique<DockArea>();
    dock->set_center(std::make_unique<Label>("center"));
    dock->add_dock(DockPosition::South, "Console", std::make_unique<Button>("Focusable content"));
    auto south = dock->dock_tab_widget(DockPosition::South);

    win.set_root(std::move(dock));
    win.relayout();

    south->set_collapsed(true);
    REQUIRE(south->is_collapsed());

    auto origin = south->map_to_window({0, 0});
    auto r = south->rect();
    Point tab_bar_point{origin.x + 20.0f, origin.y + r.height / 2.0f};
    win.handle_mouse({MouseEvent::Type::Press, tab_bar_point});
    win.handle_mouse({MouseEvent::Type::Release, tab_bar_point});

    REQUIRE_FALSE(south->is_collapsed());
}
