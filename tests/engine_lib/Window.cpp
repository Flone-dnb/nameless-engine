#include "Catch2/catch.hpp"

#include "Window.h"
#include "Error.h"

TEST_CASE("create simple window") {
    // Check that window creation is working correctly.

    auto result = dxe::Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<dxe::Error>(result)) {
        INFO(std::get<dxe::Error>(std::move(result)).getError())
        REQUIRE(false);
    }
    REQUIRE(true);
}

TEST_CASE("create 2 windows") {
    // Check that window class names are unique.

    auto result1 = dxe::Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<dxe::Error>(result1)) {
        INFO(std::get<dxe::Error>(std::move(result1)).getError())
        REQUIRE(false);
    }

    auto result2 = dxe::Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<dxe::Error>(result2)) {
        INFO(std::get<dxe::Error>(std::move(result2)).getError())
        REQUIRE(false);
    }

    REQUIRE(true);
}