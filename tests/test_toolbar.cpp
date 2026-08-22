// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/button.hpp"
#include "svision3/layout.hpp"
#include "svision3/theme.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/toolbar.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

// Toolbar::size_hint() is its layout's, so anything that changes the entries
// changes what the toolbar asks for. It used to rebuild the layout and, at most,
// request a repaint -- leaving ancestors sized for the contents it no longer has.
// A toolbar whose items are swapped at runtime hits this on every swap.
TEST_CASE("Toolbar content changes invalidate the layout", "[toolbar]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::Win11, ColorScheme::Light));

    auto parent = std::make_shared<VBoxLayout>();
    auto bar = std::make_shared<Toolbar>();
    parent->add_widget(bar);
    bar->set_rect({0, 0, 400, 32});

    auto settle = [&] {
        parent->set_layout_dirty(false);
        bar->set_layout_dirty(false);
    };

    SECTION("adding a command") {
        auto before = bar->size_hint();
        settle();

        bar->add_command(Command::create("Undo", [] {}));

        REQUIRE(bar->size_hint().width != before.width); // the hint really changed
        REQUIRE(bar->is_layout_dirty());
        REQUIRE(parent->is_layout_dirty());
    }

    SECTION("removing a range") {
        bar->add_command(Command::create("Undo", [] {}));
        bar->add_command(Command::create("Redo", [] {}));
        auto before = bar->size_hint();
        settle();

        bar->remove_range(0, 1);

        REQUIRE(bar->size_hint().width != before.width);
        REQUIRE(bar->is_layout_dirty());
        REQUIRE(parent->is_layout_dirty());
    }

    SECTION("changing orientation") {
        bar->add_command(Command::create("Undo", [] {}));
        settle();

        bar->set_orientation(Orientation::Vertical);

        REQUIRE(bar->is_layout_dirty());
        REQUIRE(parent->is_layout_dirty());
    }

    SECTION("clearing") {
        bar->add_command(Command::create("Undo", [] {}));
        settle();

        bar->clear();

        REQUIRE(bar->is_layout_dirty());
        REQUIRE(parent->is_layout_dirty());
    }
}
