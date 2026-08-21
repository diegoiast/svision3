// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/combobox.hpp"
#include "svision3/layout.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/window.hpp"
#include <catch2/catch_test_macros.hpp>
#include <svision3/text_rasterizer.hpp>

using namespace svision3;

namespace {

struct Fixture {
    Window win{"Test Combobox", {400, 300}};
    Combobox *combo = nullptr;

    Fixture() {
        Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

        auto root = std::make_unique<VBoxLayout>();
        auto box = std::make_unique<Combobox>(std::vector<std::string>{"One", "Two", "Three"});
        combo = box.get();

        root->add_widget(std::move(box));
        win.set_root(std::move(root));
        win.handle_resize({400, 300});

        MockPainter painter(new DummyRasterizer);
        win.handle_paint(painter);
    }
};

} // namespace

TEST_CASE("Combobox dropdown registers as a window popup", "[combobox][popup]") {
    Fixture f;
    REQUIRE(!f.win.has_popup());

    f.combo->open_dropdown();
    REQUIRE(f.win.has_popup());
    REQUIRE(f.combo->is_open());

    f.combo->close_dropdown();
    REQUIRE(!f.win.has_popup());
    REQUIRE(!f.combo->is_open());
}

TEST_CASE("Closing popups externally clears the combobox open state", "[combobox][popup]") {
    Fixture f;

    f.combo->open_dropdown();
    REQUIRE(f.combo->is_open());

    // The dropdown is dismissed by the window, not by the combobox -- an outside
    // click or a parent menu going away takes this path.
    f.win.close_all_popups();
    REQUIRE(!f.win.has_popup());
    REQUIRE(!f.combo->is_open());
}

TEST_CASE("Combobox can be reopened after an external dismissal", "[combobox][popup]") {
    Fixture f;

    f.combo->open_dropdown();
    f.win.close_all_popups();

    // The stale open_ left behind by an external dismiss used to make this open a
    // second popup on top of a widget that already believed it was open.
    f.combo->open_dropdown();
    REQUIRE(f.win.has_popup());
    REQUIRE(f.combo->is_open());
}

TEST_CASE("Closing a combobox dropdown twice pops only one popup", "[combobox][popup]") {
    Fixture f;

    // A menu underneath the combobox: closing the dropdown must not take it too.
    auto sentinel = Popup{};
    sentinel.bounds = {0, 0, 10, 10};
    f.win.open_popup(std::move(sentinel));

    f.combo->open_dropdown();
    f.combo->close_dropdown();
    f.combo->close_dropdown();

    REQUIRE(f.win.has_popup());
}
