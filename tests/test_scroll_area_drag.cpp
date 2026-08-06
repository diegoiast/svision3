#include <catch2/catch_test_macros.hpp>
#include "toolkit/scroll_area.hpp"
#include "toolkit/window.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"

using namespace toolkit;

namespace {

// A trivial widget with a fixed, oversized natural size, just to force
// ScrollArea to grow a vertical scrollbar.
class BigWidget : public Widget {
  public:
    std::string_view class_name() const override { return "BigWidget"; }
    Size size_hint() const override { return {2000, 4000}; }
    void paint(Painter &) override {}
    bool handle_mouse(MouseEvent const &) override { return false; }
};

} // namespace

// Regression test for 07c85a5e ("Mouse clicks: send only to relevant widgets"):
// ScrollArea's inner Scrollbar is deliberately non-focusable (set_focusable(false),
// so Tab doesn't stop on it), which means dragging its thumb is never captured via
// Window's find_focusable_at()-based captured_widget_ path. It only received Move/
// Drag continuation because Window used to broadcast those events to the whole tree
// regardless of pointer position. Once that broadcast was replaced with a bounds-
// checked widget_at() lookup, dragging the thumb outside the window silently
// stopped updating -- even though the scrollbar had already grabbed the OS pointer
// and expects to keep tracking (see Scrollbar::handle_mouse's is_dragging() checks
// and ScrollableWidget::handle_scrollbar_mouse's out-of-rect fallback for a
// dragging scrollbar).
TEST_CASE("ScrollArea scrollbar drag continues outside window bounds", "[scroll_area][mouse]") {
    // Non-overlay scrollbars only: with inline (overlay) scrollbars, ScrollArea's
    // vscroll_ never gets a real rect_ (see ScrollableWidget::layout_scrollbars),
    // so it isn't a useful vehicle for testing this specific regression.
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win95, ColorScheme::Light));

    Window win("Test", {800, 600});

    auto area = std::make_unique<ScrollArea>();
    auto *area_ptr = area.get();
    area->set_content(std::make_unique<BigWidget>());

    win.set_root(std::move(area));
    win.relayout();

    REQUIRE(area_ptr->scroll_y() == 0.0f);

    auto rect = area_ptr->rect();
    float thumb_x = rect.x + rect.width - 5.0f;

    // Find the thumb by scanning: press, nudge, and see if scroll_y moved.
    float found_y = -1;
    for (float y = rect.y + 1; y < rect.y + rect.height - 1; y += 1.0f) {
        MouseEvent press{};
        press.type = MouseEvent::Type::Press;
        press.button = 0;
        press.position = {thumb_x, y};
        win.handle_mouse(press);

        MouseEvent drag{};
        drag.type = MouseEvent::Type::Drag;
        drag.position = {thumb_x, y + 5.0f};
        win.handle_mouse(drag);

        if (area_ptr->scroll_y() != 0.0f) {
            found_y = y;
            break;
        }

        MouseEvent release{};
        release.type = MouseEvent::Type::Release;
        release.position = {thumb_x, y};
        win.handle_mouse(release);
    }
    REQUIRE(found_y >= 0.0f);

    auto in_window_scroll = area_ptr->scroll_y();
    REQUIRE(in_window_scroll > 0.0f);

    // Drag above the window entirely -- should clamp to the top (0).
    MouseEvent drag_above{};
    drag_above.type = MouseEvent::Type::Drag;
    drag_above.position = {thumb_x, -500.0f};
    win.handle_mouse(drag_above);
    REQUIRE(area_ptr->scroll_y() == 0.0f);

    // Drag far below the window -- should clamp to the max scroll offset.
    MouseEvent drag_below{};
    drag_below.type = MouseEvent::Type::Drag;
    drag_below.position = {thumb_x, 5000.0f};
    win.handle_mouse(drag_below);
    REQUIRE(area_ptr->scroll_y() > in_window_scroll);

    MouseEvent release{};
    release.type = MouseEvent::Type::Release;
    release.position = {thumb_x, 5000.0f};
    win.handle_mouse(release);
}
