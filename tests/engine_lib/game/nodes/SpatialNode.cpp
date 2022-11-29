// Custom.
#include "game/nodes/SpatialNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("world location, rotation and scale are calculated correctly (no parent)") {
    using namespace ne;

    constexpr float floatEpsilon = 0.00001f;

    {
        const auto targetWorldLocation = glm::vec3(1.0f, 2.0f, 3.0f);
        const auto targetWorldRotation = glm::vec3(10.0f, 20.0f, 30.0f);
        const auto targetWorldScale = glm::vec3(5.0f, 6.0f, 7.0f);

        const auto pSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");
        pSpatialNode->setRelativeLocation(targetWorldLocation);
        pSpatialNode->setRelativeRotation(targetWorldRotation);
        pSpatialNode->setRelativeScale(targetWorldScale);

        const auto worldLocation = pSpatialNode->getWorldLocation();
        const auto worldRotation = pSpatialNode->getWorldRotation();
        const auto worldScale = pSpatialNode->getWorldScale();

        REQUIRE(glm::all(glm::epsilonEqual(worldLocation, targetWorldLocation, floatEpsilon)));
        REQUIRE(glm::all(glm::epsilonEqual(worldRotation, targetWorldRotation, floatEpsilon)));
        REQUIRE(glm::all(glm::epsilonEqual(worldScale, targetWorldScale, floatEpsilon)));
    }

    gc_collector()->collect();
}

TEST_CASE("world location, rotation and scale are calculated correctly (with parent)") {
    using namespace ne;

    constexpr float floatEpsilon = 0.00001f;

    {
        const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Parent Spatial Node");
        pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
        pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 1.0f, 1.0f));

        const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node");
        pParentSpatialNode->addChildNode(pChildSpatialNode);
        pChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
        pChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

        const auto worldLocation = pChildSpatialNode->getWorldLocation();
        const auto worldRotation = pChildSpatialNode->getWorldRotation();
        const auto worldScale = pChildSpatialNode->getWorldScale();

        REQUIRE(glm::all(glm::epsilonEqual(worldLocation, glm::vec3(10.0f, 0.0f, 0.0f), floatEpsilon)));
        REQUIRE(glm::all(glm::epsilonEqual(worldRotation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
        REQUIRE(glm::all(glm::epsilonEqual(worldScale, glm::vec3(5.0f, 1.0f, 5.0f), floatEpsilon)));
    }

    gc_collector()->collect();
}
