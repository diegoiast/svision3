// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/text_rasterizer.hpp"
#include <catch2/catch_session.hpp>

using namespace toolkit;

int main(int argc, char *argv[]) {
    DummyPlatformApplication app;
    // FIXME: should we set it directly in the app?
    app.set_rasterizer(new DummyRasterizer);
    detail::set_current_platform(&app);
    int result = Catch::Session().run(argc, argv);
    detail::set_current_platform(nullptr);
    return result;
}
