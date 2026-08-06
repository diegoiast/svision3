// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "toolkit/window.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/label.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"

using namespace toolkit;

// Regression test: every theme's WindowTitleBar subclass builds its own internal HBoxLayout for
// the icon/title/min/max/close row (each overrides initializeTitleBar() separately -- there is
// no shared construction path). That layout's `parent_` must be wired up to the title bar (done
// in WindowTitleBar::set_rect(), the one method every theme *does* share) so map_to_window() /
// map_from_window() on a button can walk all the way out to the window and pick up the CSD
// border+shadow inset. When that wiring was missing, Window::handle_mouse()'s single-target
// dispatch resolved a click straight to the button but handed it garbage local coordinates, so
// the click silently missed hit_test() and the button never fired -- for every theme.
//
// Exercised through the real Window::handle_mouse() dispatch path (not internal APIs), on the
// headless dummy platform, so it needs no display and would have caught the regression outright.
TEST_CASE("CSD title bar maximize button is clickable in every theme", "[window][titlebar]") {
    // GNOME's title bar (GnomeTitleBar::initializeTitleBar(), theme_factory.cpp) only builds a
    // close button by design -- no min/max -- so it has nothing for this assertion to click.
    // The fix itself is still exercised for GNOME: it lives in the one method every theme's
    // title bar shares unconditionally, WindowTitleBar::set_rect() (not overridden per theme,
    // unlike initializeTitleBar()), so coverage here for the other themes covers GNOME's layout
    // wiring too.
    auto style = GENERATE(ThemeStyle::Win11, ThemeStyle::Win95, ThemeStyle::Plasma6,
                          ThemeStyle::MacOS);
    CAPTURE(static_cast<int>(style));
    Theme::set_current(ThemeFactory::create(style, ColorScheme::Light));

    WindowOptions opts{};
    opts.csd = true;
    Window win("Test", {800, 600}, opts);

    auto content = std::make_unique<VBoxLayout>();
    content->add_widget(std::make_unique<Label>("hello"));
    win.set_root(std::move(content));

    auto const &s = Theme::current().style;
    auto inset = s.border_width + s.shadow.size;
    auto titlebar_top = inset;
    auto titlebar_h = s.window_decoration.top;
    REQUIRE(titlebar_h > 0.0f);

    // The maximize button toggles Window::is_maximized(), which is the one titlebar-button
    // effect observable through the public API without reaching into theme-private widgets --
    // scan for it by dispatching real Press/Release events, exactly as platform input would.
    auto click_at = [&](Point p) {
        MouseEvent press{};
        press.type = MouseEvent::Type::Press;
        press.position = p;
        win.handle_mouse(press);
        MouseEvent release = press;
        release.type = MouseEvent::Type::Release;
        win.handle_mouse(release);
    };

    auto find_maximize_toggle = [&]() -> Point {
        for (float y = titlebar_top; y < titlebar_top + titlebar_h; y += 1.0f) {
            for (float x = 0; x < win.size().width; x += 1.0f) {
                auto before = win.is_maximized();
                click_at({x, y});
                if (win.is_maximized() != before) {
                    return {x, y};
                }
            }
        }
        return {-1, -1};
    };

    auto was_maximized = win.is_maximized();
    auto hit = find_maximize_toggle();
    CAPTURE(hit.x, hit.y);
    REQUIRE(hit.x >= 0.0f);
    REQUIRE(win.is_maximized() != was_maximized);

    // Clicking the same spot again should restore -- same button, same coordinate mapping.
    click_at(hit);
    REQUIRE(win.is_maximized() == was_maximized);
}

// Regression test: WindowTitleBar::widget_at() only falls back to itself (so double-click-to-
// maximize / drag-to-move can run) when the child it would otherwise resolve into opts out via
// Widget::blocks_hit_test() -- see the comment on that method in window_title_bar.cpp. Label is
// the one child stretched to fill the whole draggable middle of the bar (stretch factor 1,
// Alignment::Center), so it decides whether that resolves back to WindowTitleBar or gets
// swallowed by a purely-decorative Label whose handle_mouse() is a no-op. Label's override of
// blocks_hit_test() was left commented out, so this silently regressed: double-click and window
// drag stopped working whenever the click landed on the title text itself (as opposed to bare
// bar background beside it) -- inconsistent because it depends on title length vs bar width.
TEST_CASE("CSD title bar double-click-to-maximize works when clicking the title text", "[window][titlebar]") {
    auto style = GENERATE(ThemeStyle::Win11, ThemeStyle::Win95, ThemeStyle::Plasma6,
                          ThemeStyle::MacOS);
    CAPTURE(static_cast<int>(style));
    Theme::set_current(ThemeFactory::create(style, ColorScheme::Light));

    WindowOptions opts{};
    opts.csd = true;
    Window win("A reasonably long window title", {800, 600}, opts);

    auto content = std::make_unique<VBoxLayout>();
    content->add_widget(std::make_unique<Label>("hello"));
    win.set_root(std::move(content));

    auto const &s = Theme::current().style;
    auto inset = s.border_width + s.shadow.size;
    auto titlebar_top = inset;
    auto titlebar_h = s.window_decoration.top;
    REQUIRE(titlebar_h > 0.0f);

    // The horizontal center of the bar sits under the title label (it is stretched to fill the
    // space between the leading icon and the trailing buttons), well clear of any button.
    Point center{win.size().width / 2.0f, titlebar_top + titlebar_h / 2.0f};

    auto was_maximized = win.is_maximized();
    MouseEvent press{};
    press.type = MouseEvent::Type::Press;
    press.click_count = 2;
    press.position = center;
    win.handle_mouse(press);
    MouseEvent release = press;
    release.type = MouseEvent::Type::Release;
    win.handle_mouse(release);

    REQUIRE(win.is_maximized() != was_maximized);
}
