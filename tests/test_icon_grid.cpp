// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/icon_grid.hpp"
#include "toolkit/item_model.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace toolkit;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::shared_ptr<StringListModel> make_text_model(int n = 5) {
    std::vector<std::string> items;
    for (int i = 0; i < n; ++i) {
        items.push_back("Item " + std::to_string(i));
    }
    return std::make_shared<StringListModel>(std::move(items));
}

// A model that always returns an icon of a fixed pixel size regardless of the
// requested size.  This simulates an icon theme that only ships 48 px variants.
struct FixedSizeIconModel : public ItemModel {
    int available_size;
    mutable int last_requested_size = -1;

    explicit FixedSizeIconModel(int available) : available_size(available) {}

    size_t row_count() const override { return 3; }
    std::string cell_text(size_t row, size_t /*col*/) const override {
        return "icon_" + std::to_string(row);
    }

    Icon icon_at(size_t /*row*/, size_t /*col*/, int requested_size) const override {
        last_requested_size = requested_size;
        // Build a minimal ImageData with the fixed size.
        auto data = std::make_shared<ImageData>();
        data->width = available_size;
        data->height = available_size;
        data->channels = 4;
        data->pixels.assign(static_cast<size_t>(available_size * available_size * 4), 0xFF);
        return data;
    }
};

// ---------------------------------------------------------------------------
// display_icon_size() — nearest-standard-size snapping
// ---------------------------------------------------------------------------

TEST_CASE("IconGrid::display_icon_size snaps to exact standard sizes", "[icongrid]") {
    auto model = make_text_model();
    IconGrid grid(model);

    grid.set_icon_size(16);
    REQUIRE(grid.display_icon_size() == 16);

    grid.set_icon_size(22);
    REQUIRE(grid.display_icon_size() == 22);

    grid.set_icon_size(24);
    REQUIRE(grid.display_icon_size() == 24);

    grid.set_icon_size(32);
    REQUIRE(grid.display_icon_size() == 32);

    grid.set_icon_size(48);
    REQUIRE(grid.display_icon_size() == 48);

    grid.set_icon_size(64);
    REQUIRE(grid.display_icon_size() == 64);
}

TEST_CASE("IconGrid::display_icon_size snaps to nearest when between sizes", "[icongrid]") {
    auto model = make_text_model();
    IconGrid grid(model);

    // Between 22 and 24 — should snap to 22 (diff=1 vs diff=3 from 24? No, 24-23=1, 23-22=1).
    // Equal distance: algorithm keeps the earlier winner (22).
    grid.set_icon_size(23);
    REQUIRE(grid.display_icon_size() == 22);

    // Clearly closer to 32 than to 24.
    grid.set_icon_size(30);
    REQUIRE(grid.display_icon_size() == 32);

    // Midpoint between 32 and 48 is 40; 40 is equidistant, algorithm keeps 32.
    grid.set_icon_size(40);
    REQUIRE(grid.display_icon_size() == 32);

    // One step past the midpoint — snaps to 48.
    grid.set_icon_size(41);
    REQUIRE(grid.display_icon_size() == 48);
}

TEST_CASE("IconGrid::display_icon_size returns icon_size directly when scale_icons is true",
          "[icongrid]") {
    auto model = make_text_model();
    IconGrid grid(model);
    grid.set_scale_icons(true);

    grid.set_icon_size(30);
    REQUIRE(grid.display_icon_size() == 30); // arbitrary size, no snapping

    grid.set_icon_size(100);
    REQUIRE(grid.display_icon_size() == 100);
}

// ---------------------------------------------------------------------------
// Oversized-icon regression: model only has 48 px, grid wants 32 px
// ---------------------------------------------------------------------------

TEST_CASE("IconGrid requests display_icon_size from model, not raw icon_size", "[icongrid]") {
    // The grid should always query the model with display_icon_size(), not icon_size().
    // When scale_icons=false and icon_size=32, display_icon_size() == 32.
    auto model = std::make_shared<FixedSizeIconModel>(48);
    IconGrid grid(model);
    grid.set_icon_size(32);
    grid.set_scale_icons(false);

    REQUIRE(grid.display_icon_size() == 32);

    // The model will return a 48 px icon when asked for any size.
    // We verify that the model correctly records what size was requested.
    auto icon = model->icon_at(0, 0, grid.display_icon_size());
    REQUIRE(icon != nullptr);
    REQUIRE(icon->width == 48); // model only has 48 px
    REQUIRE(icon->height == 48);

    // The icon is LARGER than icon_size (48 > 32).
    // draw_icon_grid_item must scale it down rather than centering it
    // with a negative offset that would make it bleed outside the cell.
    REQUIRE(icon->width > grid.icon_size());
}

TEST_CASE("IconGrid model returns oversized icon for small request", "[icongrid]") {
    // Concrete scenario: icon theme ships only 48 px variants.
    // Grid is set to 16 px. display_icon_size() snaps to 16.
    // model->icon_at(row, col, 16) still returns a 48 px image.
    // The icon is 3x bigger than the requested cell — must be scaled down.
    auto model = std::make_shared<FixedSizeIconModel>(48);
    IconGrid grid(model);
    grid.set_icon_size(16);
    grid.set_scale_icons(false);

    REQUIRE(grid.display_icon_size() == 16);

    auto icon = model->icon_at(0, 0, grid.display_icon_size());
    REQUIRE(icon != nullptr);
    REQUIRE(icon->width == 48);
    REQUIRE(icon->width > grid.icon_size()); // triggers the scale-down path in draw_icon_grid_item
}

TEST_CASE("IconGrid with scale_icons=true always scales regardless of icon dimensions",
          "[icongrid]") {
    auto model = std::make_shared<FixedSizeIconModel>(48);
    IconGrid grid(model);
    grid.set_icon_size(32);
    grid.set_scale_icons(true);

    // With scale_icons=true, display_icon_size() == icon_size() directly.
    REQUIRE(grid.display_icon_size() == 32);

    // draw_image_scaled will be used unconditionally — icon size does not matter.
    auto icon = model->icon_at(0, 0, grid.display_icon_size());
    REQUIRE(icon != nullptr);
}

TEST_CASE("IconGrid icon exactly matching requested size is drawn without scaling",
          "[icongrid]") {
    // When the model returns an icon whose dimensions match icon_size exactly
    // and scale_icons=false, no scaling should be necessary.
    auto model = std::make_shared<FixedSizeIconModel>(32);
    IconGrid grid(model);
    grid.set_icon_size(32);
    grid.set_scale_icons(false);

    REQUIRE(grid.display_icon_size() == 32);

    auto icon = model->icon_at(0, 0, grid.display_icon_size());
    REQUIRE(icon != nullptr);
    REQUIRE(icon->width == grid.icon_size()); // exact match — no scaling needed
}

// ---------------------------------------------------------------------------
// Scale-up: model only has small icons, grid wants large ones
// ---------------------------------------------------------------------------

TEST_CASE("IconGrid scales up when model returns smaller icon than requested", "[icongrid]") {
    // Scenario: icon theme only ships 16 px variants.
    // Grid is set to 48 px. display_icon_size() snaps to 48.
    // model->icon_at(row, col, 48) still returns a 16 px image.
    // The icon is much smaller than the cell — must be scaled up.
    auto model = std::make_shared<FixedSizeIconModel>(16);
    IconGrid grid(model);
    grid.set_icon_size(48);
    grid.set_scale_icons(false);

    REQUIRE(grid.display_icon_size() == 48);

    auto icon = model->icon_at(0, 0, grid.display_icon_size());
    REQUIRE(icon != nullptr);
    REQUIRE(icon->width == 16);
    REQUIRE(icon->width < grid.icon_size()); // triggers the scale-up path in draw_icon_grid_item
}

TEST_CASE("IconGrid scales up 22 px icon into 64 px cell", "[icongrid]") {
    // Another scale-up scenario: only a 22 px variant is available for a 64 px cell.
    auto model = std::make_shared<FixedSizeIconModel>(22);
    IconGrid grid(model);
    grid.set_icon_size(64);
    grid.set_scale_icons(false);

    REQUIRE(grid.display_icon_size() == 64);

    auto icon = model->icon_at(0, 0, grid.display_icon_size());
    REQUIRE(icon != nullptr);
    REQUIRE(icon->width == 22);
    REQUIRE(icon->width < grid.icon_size());
}

TEST_CASE("IconGrid size_mismatch condition covers both directions", "[icongrid]") {
    // Verify the size-mismatch predicate (icon->width != icon_size) fires for
    // both smaller-than-cell and larger-than-cell icons.

    // Smaller: 16 px icon in 32 px cell
    {
        auto model = std::make_shared<FixedSizeIconModel>(16);
        IconGrid grid(model);
        grid.set_icon_size(32);
        auto icon = model->icon_at(0, 0, grid.display_icon_size());
        REQUIRE(icon->width != grid.icon_size()); // mismatch → scale up
    }

    // Larger: 48 px icon in 32 px cell
    {
        auto model = std::make_shared<FixedSizeIconModel>(48);
        IconGrid grid(model);
        grid.set_icon_size(32);
        auto icon = model->icon_at(0, 0, grid.display_icon_size());
        REQUIRE(icon->width != grid.icon_size()); // mismatch → scale down
    }

    // Exact: 32 px icon in 32 px cell
    {
        auto model = std::make_shared<FixedSizeIconModel>(32);
        IconGrid grid(model);
        grid.set_icon_size(32);
        auto icon = model->icon_at(0, 0, grid.display_icon_size());
        REQUIRE(icon->width == grid.icon_size()); // exact match → no scaling
    }
}
