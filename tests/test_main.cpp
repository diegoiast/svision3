// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include <catch2/catch_session.hpp>
#include "toolkit/platform/dummy_platform.hpp"

using namespace toolkit;

int main(int argc, char *argv[]) {
    DummyPlatformApplication app;
    detail::set_current_platform(&app);
    int result = Catch::Session().run(argc, argv);
    detail::set_current_platform(nullptr);
    return result;
}
