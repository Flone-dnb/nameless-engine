// Custom.
#include "game/camera/CameraProperties.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("make sure free camera rotation behaves correctly") {
    using namespace ne;

    // Setup.
    CameraProperties cameraProperties;
    cameraProperties.setDontFlipCamera(true);
    constexpr auto floatDelta = 0.001F;

    // Check initial parameters.
    const auto upDirection = cameraProperties.getUpDirection(true);
    const auto forwardDirection = cameraProperties.getForwardDirection(true);

    REQUIRE(glm::all(glm::epsilonEqual(upDirection, worldUpDirection, floatDelta)));
    REQUIRE(glm::all(glm::epsilonEqual(forwardDirection, worldForwardDirection, floatDelta)));
    REQUIRE(glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), 0.0F, floatDelta));

    // Look slightly down.
    cameraProperties.setFreeCameraPitch(-45.0F); // NOLINT: magic number

    // Check.
    REQUIRE(
        glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), -45.0F, floatDelta)); // NOLINT: magic number
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getForwardDirection(true)), 1.0F, floatDelta));
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getUpDirection(true)), 1.0F, floatDelta));

    // Look down.
    cameraProperties.setFreeCameraPitch(-90.0F); // NOLINT: magic number

    // Check.
    REQUIRE(glm::all(
        glm::epsilonEqual(cameraProperties.getUpDirection(true), -worldForwardDirection, floatDelta)));
    REQUIRE(glm::all(
        glm::epsilonEqual(cameraProperties.getForwardDirection(true), worldUpDirection, floatDelta)));
    REQUIRE(glm::all(
        glm::epsilonEqual(cameraProperties.getRightDirection(true), worldRightDirection, floatDelta)));
    REQUIRE(
        glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), -90.0F, floatDelta)); // NOLINT: magic number
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getForwardDirection(true)), 1.0F, floatDelta));
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getUpDirection(true)), 1.0F, floatDelta));
    REQUIRE(glm::epsilonEqual(glm::length(cameraProperties.getRightDirection(true)), 1.0F, floatDelta));

    // Try to flip camera.
    cameraProperties.setFreeCameraPitch(-180.0F); // NOLINT: magic number

    // Check that everything is the same.
    REQUIRE(
        glm::epsilonEqual(cameraProperties.getFreeCameraPitch(), -90.0F, floatDelta)); // NOLINT: magic number
}

TEST_CASE("make sure orbital rotation behaves correctly") {
    using namespace ne;

    // Setup.
    CameraProperties cameraProperties;
    cameraProperties.setCameraMode(CameraMode::ORBITAL);
    cameraProperties.setOrbitalCameraRotation(90.0F, 0.0F); // NOLINT: magic number
    constexpr auto floatDelta = 0.001F;

    const auto cameraTargetLocation = glm::vec3(5.0F, 0.0F, 0.0F);
    cameraProperties.setCameraLocation(glm::vec3(0.0F, 0.0F, 0.0F));
    cameraProperties.setOrbitalCameraTargetPoint(cameraTargetLocation);

    const auto cameraForward = cameraProperties.getForwardDirection(false);
    REQUIRE(glm::all(glm::epsilonEqual(cameraForward, glm::vec3(1.0F, 0.0F, 0.0F), floatDelta)));

    const auto cameraDistance = 10.0F;
    cameraProperties.setOrbitalCameraDistanceToTarget(cameraDistance); // causes location to change
    cameraProperties.setOrbitalCameraRotation(-90.0F, 0.0F);           // NOLINT: causes location to change

    const auto cameraTheta = cameraProperties.getOrbitalCameraTheta();
    REQUIRE(glm::epsilonEqual(cameraTheta, 0.0F, floatDelta));

    auto cameraLocation = cameraProperties.getLocation(false);
    REQUIRE(glm::all(
        glm::epsilonEqual(cameraLocation, glm::vec3(-5.0F, 0.0F, 0.0F), floatDelta))); // NOLINT: magic number
    REQUIRE(
        glm::epsilonEqual(cameraProperties.getOrbitalCameraDistanceToTarget(), cameraDistance, floatDelta));
    REQUIRE(glm::all(glm::epsilonEqual(
        cameraProperties.getOrbitalCameraTargetLocation(false), cameraTargetLocation, floatDelta)));
}
