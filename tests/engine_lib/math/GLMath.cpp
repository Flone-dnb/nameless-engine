// Custom.
#include "math/GLMath.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("move a point in space by a translation matrix") {
    constexpr auto floatDelta = 0.00001F;

    const auto translationMatrix = glm::translate(glm::vec3(1.0F, 2.0F, 3.0F));
    const auto point = glm::vec3(0.0F, 0.0F, 0.0F);

    glm::vec3 result = translationMatrix * glm::vec4(point, 1.0F);
    REQUIRE(glm::all(glm::epsilonEqual(result, glm::vec3(1.0F, 2.0F, 3.0F), floatDelta))); // NOLINT

    // Try incorrect order.
    result = glm::vec4(point, 1.0F) * translationMatrix;
    REQUIRE(glm::all(glm::epsilonEqual(result, glm::vec3(0.0F, 0.0F, 0.0F), floatDelta)));
}

TEST_CASE("rotate vector around X axis") {
    constexpr auto floatDelta = 0.00001F;

    const auto rotationMatrix = glm::rotate(glm::radians(90.0F), glm::vec3(1.0F, 0.0F, 0.0F));
    const auto direction = glm::vec3(0.0F, 1.0F, 0.0F);

    const glm::vec3 result = rotationMatrix * glm::vec4(direction, 0.0F);
    REQUIRE(glm::all(glm::epsilonEqual(result, glm::vec3(0.0F, 0.0F, 1.0F), floatDelta)));
}

TEST_CASE("rotate vector around Y axis") {
    constexpr auto floatDelta = 0.00001F;

    const auto rotationMatrix = glm::rotate(glm::radians(90.0F), glm::vec3(0.0F, 1.0F, 0.0F));
    const auto direction = glm::vec3(0.0F, 0.0F, 1.0F);

    const glm::vec3 result = rotationMatrix * glm::vec4(direction, 0.0F);
    REQUIRE(glm::all(glm::epsilonEqual(result, glm::vec3(1.0F, 0.0F, 0.0F), floatDelta)));
}

TEST_CASE("rotate vector around Z axis") {
    constexpr auto floatDelta = 0.00001F;

    const auto rotationMatrix = glm::rotate(glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
    const auto direction = glm::vec3(1.0F, 0.0F, 0.0F);

    const glm::vec3 result = rotationMatrix * glm::vec4(direction, 0.0F);
    REQUIRE(glm::all(glm::epsilonEqual(result, glm::vec3(0.0F, 1.0F, 0.0F), floatDelta)));
}
