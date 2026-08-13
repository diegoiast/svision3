#include "svision3/progress_bar.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

TEST_CASE("ProgressBar default value is zero", "[progressbar]") {
    ProgressBar pb;
    REQUIRE(pb.value() == 0.0f);
}

TEST_CASE("ProgressBar set_value", "[progressbar]") {
    ProgressBar pb;
    pb.set_value(0.5f);
    REQUIRE(pb.value() == 0.5f);
}

TEST_CASE("ProgressBar clamps to [0,1]", "[progressbar]") {
    ProgressBar pb;
    pb.set_value(-0.5f);
    REQUIRE(pb.value() == 0.0f);

    pb.set_value(1.5f);
    REQUIRE(pb.value() == 1.0f);

    pb.set_value(0.0f);
    REQUIRE(pb.value() == 0.0f);

    pb.set_value(1.0f);
    REQUIRE(pb.value() == 1.0f);
}

TEST_CASE("ProgressBar is not focusable", "[progressbar]") {
    ProgressBar pb;
    REQUIRE(pb.is_focusable() == false);
}

TEST_CASE("ProgressBar mouse event returns false", "[progressbar]") {
    ProgressBar pb;
    pb.set_rect({0, 0, 200, 20});
    MouseEvent e{};
    e.type = MouseEvent::Type::Press;
    e.position = {100, 10};
    REQUIRE(pb.handle_mouse(e) == false);
}
