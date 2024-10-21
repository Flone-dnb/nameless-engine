// Custom.
#include "math/MathHelpers.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("get roll/pitch/yaw from +X direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::vec3(1.0F, 0.0F, 0.0F);

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, 0.0F), floatDelta)));

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from -X direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::vec3(-1.0F, 0.0F, 0.0F);

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, 180.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from +Y direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::vec3(0.0F, 1.0F, 0.0F);

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, 90.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from -Y direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::vec3(0.0F, -1.0F, 0.0F);

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, -90.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from +Z direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::vec3(0.0F, 0.0F, 1.0F);

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, -90.0F, 0.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from -Z direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::vec3(0.0F, 0.0F, -1.0F);

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 90.0F, 0.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from +X+Y direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(1.0F, 1.0F, 0.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, 45.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from +X-Y direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(1.0F, -1.0F, 0.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, -45.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from -X+Y direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(-1.0F, 1.0F, 0.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, 135.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from -X-Y direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(-1.0F, -1.0F, 0.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 0.0F, -135.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from +X+Z direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(1.0F, 0.0F, 1.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, -45.0F, 0.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from -X+Z direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(-1.0F, 0.0F, 1.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, -45.0F, 180.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from +X-Z direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(1.0F, 0.0F, -1.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 45.0F, 0.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}

TEST_CASE("get roll/pitch/yaw from -X-Z direction") {
    using namespace ne;

    constexpr auto floatDelta = 0.00001F;

    const auto direction = glm::normalize(glm::vec3(-1.0F, 0.0F, -1.0F));

    // Convert to rotation.
    const auto rotation = MathHelpers::convertNormalizedDirectionToRollPitchYaw(direction);
    REQUIRE(glm::all(glm::epsilonEqual(rotation, glm::vec3(0.0F, 45.0F, 180.0F), floatDelta))); // NOLINT

    // Convert back to direction.
    const auto calculatedDirection = MathHelpers::convertRollPitchYawToDirection(rotation);
    REQUIRE(glm::all(glm::epsilonEqual(calculatedDirection, direction, floatDelta)));
}
