#include "Catch2/catch.hpp"

#include "Window.h"
#include "Error.h"

TEST_CASE("create simple window") {
    auto result = dxe::Window::newInstance();
    if (std::holds_alternative<dxe::Error>(result)) {
        INFO(std::get<dxe::Error>(std::move(result)).getError())
        REQUIRE(false);
    }
    REQUIRE(true);
}