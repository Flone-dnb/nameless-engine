// Custom.
#include "game/Window.h"
#include "misc/Error.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("create simple window") {
    // Check that window creation is working correctly.

    using namespace ne;

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        INFO(std::get<Error>(std::move(result)).getFullErrorMessage());
        REQUIRE(false);
    }
    REQUIRE(true);
}

TEST_CASE("create 2 windows") {
    // Check that window class names are unique.

    using namespace ne;

    auto result1 = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result1)) {
        INFO(std::get<Error>(std::move(result1)).getFullErrorMessage());
        REQUIRE(false);
    }

    auto result2 = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result2)) {
        INFO(std::get<Error>(std::move(result2)).getFullErrorMessage());
        REQUIRE(false);
    }

    REQUIRE(true);
}
