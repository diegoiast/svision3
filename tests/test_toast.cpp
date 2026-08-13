#include "svision3/toast_widget.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace svision3;

TEST_CASE("ToastBuilder builds toast with plain text", "[toast]") {
    auto toast = ToastBuilder().text("Hello").title("Title").timeout(5.0f).build();
    REQUIRE(toast != nullptr);
    REQUIRE(toast->size_hint().width == 300.0f);
}

TEST_CASE("ToastBuilder builds toast with rich text", "[toast]") {
    auto toast = ToastBuilder().rich_text("**bold** text").build();
    REQUIRE(toast != nullptr);
}

TEST_CASE("ToastBuilder background color is applied", "[toast]") {
    auto toast = ToastBuilder().text("Test").background(Color::rgb(1, 0, 0)).build();
    REQUIRE(toast != nullptr);
}
