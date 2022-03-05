#include "Catch2/catch.hpp"

#include "Window.h"
#include "Error.h"

TEST_CASE("create simple window") {
    auto result = dxe::Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<dxe::Error>(result)) {
        INFO(std::get<dxe::Error>(std::move(result)).getError())
        REQUIRE(false);
    }
    REQUIRE(true);
}

TEST_CASE("create window with random title") {
    auto result = dxe::Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<dxe::Error>(result)) {
        INFO(std::get<dxe::Error>(std::move(result)).getError())
        REQUIRE(false);
    }

    REQUIRE(!std::get<std::unique_ptr<dxe::Window>>(std::move(result))->getTitle().empty());
}

TEST_CASE("fail to create a window with non-unique name") {
    auto result1 = dxe::Window::getBuilder().withTitle("Main Window").withVisibility(false).build();
    if (std::holds_alternative<dxe::Error>(result1)) {
        INFO(std::get<dxe::Error>(result1).getError())
        REQUIRE(false);
    }

    auto result2 = dxe::Window::getBuilder().withTitle("Main Window").withVisibility(false).build();
    // should hold an error because this window name is not unique
    REQUIRE(std::holds_alternative<dxe::Error>(result2));
}