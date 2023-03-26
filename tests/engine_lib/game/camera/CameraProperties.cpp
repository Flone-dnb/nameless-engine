// Custom.
#include "game/camera/CameraProperties.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("make sure free camera rotation behaves correctly") {
    using namespace ne;

    // Setup.
    CameraProperties cameraProperties;
    cameraProperties.setDontFlipCamera(true);
    constexpr auto floatDelta = 0.00001F;

    // Check initial parameters.
    REQUIRE(glm::all(glm::epsilonEqual(cameraProperties.getUpDirection(), worldUpDirection, floatDelta)));
    REQUIRE(glm::all(
        glm::epsilonEqual(cameraProperties.getForwardDirection(), worldForwardDirection, floatDelta)));
    REQUIRE(glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), 0.0F, floatDelta));

    // Look slightly down.
    cameraProperties.setFreeCameraPitch(-45.0F); // NOLINT: magic number

    // Check.
    REQUIRE(
        glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), -45.0F, floatDelta)); // NOLINT: magic number
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getForwardDirection()), 1.0F, floatDelta));
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getUpDirection()), 1.0F, floatDelta));

    // Look down.
    cameraProperties.setFreeCameraPitch(-90.0F); // NOLINT: magic number

    // Check.
    REQUIRE(
        glm::all(glm::epsilonEqual(cameraProperties.getUpDirection(), worldForwardDirection, floatDelta)));
    REQUIRE(
        glm::all(glm::epsilonEqual(cameraProperties.getForwardDirection(), -worldUpDirection, floatDelta)));
    REQUIRE(
        glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), -90.0F, floatDelta)); // NOLINT: magic number
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getForwardDirection()), 1.0F, floatDelta));
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getUpDirection()), 1.0F, floatDelta));

    // Try to flip camera.
    cameraProperties.setFreeCameraPitch(-180.0F); // NOLINT: magic number

    // Check that everything is the same.
    REQUIRE(
        glm::all(glm::epsilonEqual(cameraProperties.getUpDirection(), worldForwardDirection, floatDelta)));
    REQUIRE(
        glm::all(glm::epsilonEqual(cameraProperties.getForwardDirection(), -worldUpDirection, floatDelta)));
    REQUIRE(
        glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), -90.0F, floatDelta)); // NOLINT: magic number
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getForwardDirection()), 1.0F, floatDelta));
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getUpDirection()), 1.0F, floatDelta));
}
