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

TEST_CASE("fail to create a window with non-unique name") {
    auto result1 = dxe::Window::newInstance(800, 600, "window");
    if (std::holds_alternative<dxe::Error>(result1)) {
        INFO(std::get<dxe::Error>(result1).getError())
        REQUIRE(false);
    }

    auto result2 = dxe::Window::newInstance(800, 600, "window");
    // should hold an error because this window name is not unique
    REQUIRE(std::holds_alternative<dxe::Error>(result2));
}