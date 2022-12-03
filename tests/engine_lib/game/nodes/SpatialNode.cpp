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

TEST_CASE(
    "world location, rotation and scale are calculated correctly (with non spatial nodes in the hierarchy)") {
    using namespace ne;

    constexpr float floatEpsilon = 0.00001f;

    {
        // Create nodes.
        const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Parent Spatial Node");
        pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
        pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 1.0f, 1.0f));

        const auto pUsualNode1 = gc_new<Node>("Usual Node 1");

        const auto pSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node");

        const auto pUsualNode2 = gc_new<Node>("Usual Node 2");

        const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node 1");
        pChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
        pChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

        const auto pUsualNode3 = gc_new<Node>("Usual Node 3");

        const auto pChildChildSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node 2");
        pChildChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
        pChildChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

        // Build hierarchy.
        pParentSpatialNode->addChildNode(pUsualNode1);
        pUsualNode1->addChildNode(pSpatialNode);
        pSpatialNode->addChildNode(pUsualNode2);
        pUsualNode2->addChildNode(pChildSpatialNode);
        pChildSpatialNode->addChildNode(pUsualNode3);
        pUsualNode3->addChildNode(pChildChildSpatialNode);

        // Check locations.
        const auto worldLocation = pChildChildSpatialNode->getWorldLocation();
        const auto worldRotation = pChildChildSpatialNode->getWorldRotation();
        const auto worldScale = pChildChildSpatialNode->getWorldScale();

        REQUIRE(glm::all(glm::epsilonEqual(worldLocation, glm::vec3(15.0f, 0.0f, 0.0f), floatEpsilon)));
        REQUIRE(glm::all(glm::epsilonEqual(worldRotation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
        REQUIRE(glm::all(glm::epsilonEqual(worldScale, glm::vec3(5.0f, 1.0f, 25.0f), floatEpsilon)));
    }

    gc_collector()->collect();
}

TEST_CASE("world location with parent rotation is correct") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

            float floatEpsilon = 0.00001f;

            // Create nodes.
            const auto pParentSpatialNode = gc_new<SpatialNode>();
            pParentSpatialNode->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));

            const auto pSpatialNodeA = gc_new<SpatialNode>();
            pSpatialNodeA->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));
            pSpatialNodeA->setRelativeLocation(glm::vec3(10.0f, 0.0f, 0.0f));

            const auto pSpatialNodeB = gc_new<SpatialNode>();
            pSpatialNodeB->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));

            const auto pChildSpatialNodeA = gc_new<SpatialNode>();
            const auto pChildSpatialNodeB = gc_new<SpatialNode>();
            pChildSpatialNodeB->setRelativeLocation(glm::vec3(10.0f, 0.0f, 0.0f));

            // Build hierarchy.
            pParentSpatialNode->addChildNode(pSpatialNodeA);
            pParentSpatialNode->addChildNode(pSpatialNodeB);
            pSpatialNodeA->addChildNode(pChildSpatialNodeA);
            pSpatialNodeB->addChildNode(pChildSpatialNodeB);
            getWorldRootNode()->addChildNode(pParentSpatialNode);

            // Check location.
            const auto middleANodeWorldLocation = pSpatialNodeA->getWorldLocation();
            const auto childANodeWorldLocation = pChildSpatialNodeA->getWorldLocation();
            const auto childBNodeWorldLocation = pChildSpatialNodeB->getWorldLocation();
            REQUIRE(glm::all(glm::epsilonEqual(
                middleANodeWorldLocation, glm::vec3(7.07106f, -7.07106f, 0.0f), floatEpsilon)));
            REQUIRE(glm::all(glm::epsilonEqual(
                childANodeWorldLocation, glm::vec3(7.07106f, -7.07106f, 0.0f), floatEpsilon)));
            REQUIRE(glm::all(
                glm::epsilonEqual(childBNodeWorldLocation, glm::vec3(0.0f, -10.0f, 0.0f), floatEpsilon)));

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("set world location with parent is correct") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

            constexpr float floatEpsilon = 0.00001f;

            // Create nodes.
            const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");
            pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 5.0f, 5.0f));

            const auto pUsualNode = gc_new<Node>("Usual Node");

            const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");

            // Build hierarchy.
            getWorldRootNode()->addChildNode(pParentSpatialNode);
            pParentSpatialNode->addChildNode(pUsualNode);
            pUsualNode->addChildNode(pChildSpatialNode);

            // Set world location.
            pChildSpatialNode->setWorldLocation(glm::vec3(-5.0f, -5.0f, -5.0f));

            // Check locations.
            REQUIRE(glm::all(glm::epsilonEqual(
                pParentSpatialNode->getWorldLocation(), glm::vec3(5.0f, 5.0f, 5.0f), floatEpsilon)));
            REQUIRE(glm::all(glm::epsilonEqual(
                pChildSpatialNode->getRelativeLocation(), glm::vec3(-10.0f, -10.0f, -10.0f), floatEpsilon)));
            REQUIRE(glm::all(glm::epsilonEqual(
                pChildSpatialNode->getWorldLocation(), glm::vec3(-5.0f, -5.0f, -5.0f), floatEpsilon)));

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("set world rotation with parent is correct") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

            float floatEpsilon = 0.00001f;

            // Create nodes.
            const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");
            pParentSpatialNode->setRelativeRotation(glm::vec3(1.0f, 5.0f, 10.0f));

            const auto pUsualNode = gc_new<Node>("Usual Node");

            const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");

            // Build hierarchy.
            getWorldRootNode()->addChildNode(pParentSpatialNode);
            pParentSpatialNode->addChildNode(pUsualNode);
            pUsualNode->addChildNode(pChildSpatialNode);

            // Set world rotation.
            pChildSpatialNode->setWorldRotation(glm::vec3(-1.0f, -5.0f, -10.0f));

            // Check rotations.
            REQUIRE(glm::all(glm::epsilonEqual(
                pParentSpatialNode->getWorldRotation(), glm::vec3(1.0f, 5.0f, 10.0f), floatEpsilon)));
            REQUIRE(glm::all(glm::epsilonEqual(
                pChildSpatialNode->getRelativeRotation(), glm::vec3(-2.0f, -10.0f, -20.0f), floatEpsilon)));

            floatEpsilon = 3.0f; // degrees
            const auto worldRotation = pChildSpatialNode->getWorldRotation();
            REQUIRE(
                glm::all(glm::epsilonEqual(worldRotation, glm::vec3(-1.0f, -5.0f, -10.0f), floatEpsilon)));

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
