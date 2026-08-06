// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "toolkit/window.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/label.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"

#ifdef TOOLKIT_HAS_CAIRO
#include "toolkit/painters/cairo_painter.hpp"
#include <cairo/cairo.h>
#endif

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

#ifdef TOOLKIT_HAS_CAIRO
// Regression test: WindowTitleBar::sync_button_states() only ever updated the maximize/restore
// button's *tooltip* text -- the icon glyph itself was fixed forever to whichever DecorationButton
// the theme constructed TitlebarButton with (always ::Maximize), so it never actually switched to
// the restore glyph once the window was maximized. Rendering is the only way to observe an icon
// glyph from outside window_title_bar.cpp/theme_*.cpp (neither exposes the button or its type
// publicly), so this renders a small crop around the button, off-screen with Cairo, before/after
// maximizing and asserts those pixels actually changed.
//
// Cropped rather than diffing the whole frame: maximizing also drops the CSD shadow inset and
// squares off the corners (see Window::handle_paint's `!is_maximized_` checks), which would shift
// the whole title bar and change corner pixels regardless of this bug, drowning out the signal.
// shadow.size and corner_radius are zeroed below specifically to remove that shift so the crop
// coordinates found before maximizing stay valid after.
// MacOS is deliberately excluded: its "zoom" button draws the same green traffic-light dot (and,
// even on hover, the same small square glyph) for both DecorationButton::Maximize and ::Restore
// -- see MacOSTheme::draw_window_button -- matching real macOS convention, where the zoom button
// doesn't change appearance with window state. That's correct, not an instance of this bug.
TEST_CASE("Maximize button icon changes between maximize and restore glyphs", "[window][titlebar]") {
    auto style = GENERATE(ThemeStyle::Win11, ThemeStyle::Win95, ThemeStyle::Plasma6);
    CAPTURE(static_cast<int>(style));
    auto theme = ThemeFactory::create(style, ColorScheme::Light);
    theme->style.shadow.size = 0;
    theme->style.corner_radius = 0;
    Theme::set_current(std::move(theme));

    WindowOptions opts{};
    opts.csd = true;
    Window win("Test", {500, 200}, opts);
    win.set_root(std::make_unique<Label>("hello"));

    auto const &s = Theme::current().style;
    auto titlebar_h = s.window_decoration.top;
    REQUIRE(titlebar_h > 0.0f);

    auto width = static_cast<int>(win.size().width);
    auto height = static_cast<int>(win.size().height);

    auto render = [&]() {
        auto *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        auto *cr = cairo_create(surface);
        CairoTextRasterizer rasterizer;
        CairoPainter painter(cr, &rasterizer);
        win.handle_paint(painter);
        cairo_surface_flush(surface);
        auto *data = cairo_image_surface_get_data(surface);
        auto stride = cairo_image_surface_get_stride(surface);
        std::vector<unsigned char> pixels(data, data + static_cast<size_t>(stride) * height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return std::pair{std::move(pixels), stride};
    };

    // Locate the maximize button by behavior (same technique as the double-click test above):
    // click every title-bar pixel and see which one toggles is_maximized(). This click also
    // performs the very state transition we want to compare, so it doubles as the trigger.
    Point button{-1, -1};
    for (float y = 0; y < titlebar_h && button.x < 0.0f; y += 1.0f) {
        for (float x = 0; x < static_cast<float>(width); x += 1.0f) {
            MouseEvent press{};
            press.type = MouseEvent::Type::Press;
            press.position = {x, y};
            win.handle_mouse(press);
            MouseEvent release = press;
            release.type = MouseEvent::Type::Release;
            win.handle_mouse(release);
            if (win.is_maximized()) {
                button = {x, y};
                break;
            }
        }
    }
    CAPTURE(button.x, button.y);
    REQUIRE(button.x >= 0.0f);

    // The scan above already maximized the window as a side effect of finding the button --
    // restore first so "before" captures the un-maximized icon.
    win.restore();
    REQUIRE_FALSE(win.is_maximized());
    auto [before_pixels, before_stride] = render();

    win.maximize();
    REQUIRE(win.is_maximized());
    auto [after_pixels, after_stride] = render();

    // A crop anchored on the button isolates its icon glyph from the rest of the frame (title
    // text, other buttons), which is legitimately identical either way. `button` is the
    // top-left-most pixel that toggled is_maximized() -- i.e. the button's hit-rect corner, not
    // necessarily its visual center (Win11's title buttons are 44px wide with a small centered
    // glyph) -- so the crop extends generously right/down from that corner rather than being
    // centered on it, comfortably covering every current theme's button size and glyph position.
    auto sample = [&](std::vector<unsigned char> const &pixels, int stride) {
        std::vector<unsigned char> crop;
        auto cx = static_cast<int>(button.x);
        auto cy = static_cast<int>(button.y);
        for (int dy = -4; dy <= 36; ++dy) {
            auto py = cy + dy;
            if (py < 0 || py >= height) {
                continue;
            }
            for (int dx = -4; dx <= 48; ++dx) {
                auto px = cx + dx;
                if (px < 0 || px >= width) {
                    continue;
                }
                auto offset = static_cast<size_t>(py) * static_cast<size_t>(stride) +
                             static_cast<size_t>(px) * 4;
                if (offset + 4 <= pixels.size()) {
                    crop.insert(crop.end(), pixels.begin() + static_cast<long>(offset),
                               pixels.begin() + static_cast<long>(offset + 4));
                }
            }
        }
        return crop;
    };

    auto before_crop = sample(before_pixels, before_stride);
    auto after_crop = sample(after_pixels, after_stride);
    REQUIRE_FALSE(before_crop.empty());
    REQUIRE(before_crop.size() == after_crop.size());

    // Compare via a diff count rather than REQUIRE(before_crop != after_crop): Catch2 tries to
    // pretty-print the full byte vectors on failure, which is both unreadable for a pixel buffer
    // and, at this size, has crashed its own text-wrapping (catch_textflow.cpp assertion).
    size_t differing_bytes = 0;
    for (size_t i = 0; i < before_crop.size(); ++i) {
        if (before_crop[i] != after_crop[i]) {
            ++differing_bytes;
        }
    }
    CAPTURE(differing_bytes, before_crop.size());
    REQUIRE(differing_bytes > 0);
}
#endif
