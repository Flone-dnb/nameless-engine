// Custom.
#include "game/nodes/CameraNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("camera's location/rotation is correct when used in a node hierarchy") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                constexpr float floatEpsilon = 0.001f;

                const auto pParentSpatialNode = gc_new<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0F, 0.0F, 90.0F));
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0F, 5.0F, 5.0F));

                const auto pChildCameraNode = gc_new<CameraNode>();
                const auto pCameraProperties = pChildCameraNode->getCameraProperties();
                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getForwardDirection(true), worldForwardDirection, floatEpsilon)));

                // Spawn in world.
                pParentSpatialNode->addChildNode(pChildCameraNode);
                getWorldRootNode()->addChildNode(pParentSpatialNode);

                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildCameraNode->getWorldLocation(), glm::vec3(5.0F, 0.0F, 0.0F), floatEpsilon)));

                pChildCameraNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto nodeLocation = pChildCameraNode->getWorldLocation();
                const auto location = pCameraProperties->getLocation(true);
                const auto forward = pCameraProperties->getForwardDirection(true);
                const auto right = pCameraProperties->getRightDirection(true);

                REQUIRE(
                    glm::all(glm::epsilonEqual(nodeLocation, glm::vec3(5.0F, 25.0F, 0.0F), floatEpsilon)));

                // Compare final camera data.
                REQUIRE(glm::all(glm::epsilonEqual(location, nodeLocation, floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(forward, worldRightDirection, floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(right, -worldForwardDirection, floatEpsilon)));

                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getForwardDirection(false), worldForwardDirection, floatEpsilon)));

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("orbital camera node behaves correctly when parent node rotates") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                constexpr float floatEpsilon = 0.001f;

                const auto pParentSpatialNode = gc_new<SpatialNode>();
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0F, 0.0F, 90.0F));

                const auto pChildCameraNode = gc_new<CameraNode>();
                const auto pCameraProperties = pChildCameraNode->getCameraProperties();
                pCameraProperties->setCameraMode(CameraMode::ORBITAL);

                const auto cameraTargetLocation = glm::vec3(5.0F, 0.0F, 0.0F);
                pCameraProperties->setCameraLocation(glm::vec3(0.0F, 0.0F, 0.0F));
                pCameraProperties->setOrbitalCameraTargetPoint(cameraTargetLocation);

                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getForwardDirection(false),
                    glm::vec3(1.0F, 0.0F, 0.0F),
                    floatEpsilon)));

                pCameraProperties->setOrbitalCameraDistanceToTarget(10.0F); // causes location to change
                pCameraProperties->setOrbitalCameraRotation(-90.0F, 0.0F);

                auto cameraLocation = pCameraProperties->getLocation(false);
                REQUIRE(
                    glm::all(glm::epsilonEqual(cameraLocation, glm::vec3(-5.0F, 0.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::epsilonEqual(
                    pCameraProperties->getOrbitalCameraDistanceToTarget(), 10.0F, floatEpsilon));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getOrbitalCameraTargetLocation(false),
                    cameraTargetLocation,
                    floatEpsilon)));

                // Spawn in world.
                pParentSpatialNode->addChildNode(pChildCameraNode);
                getWorldRootNode()->addChildNode(pParentSpatialNode);

                auto targetLocation = pCameraProperties->getOrbitalCameraTargetLocation(true);
                cameraLocation = pCameraProperties->getLocation(true);
                REQUIRE(
                    glm::all(glm::epsilonEqual(targetLocation, glm::vec3(0.0F, 5.0F, 0.0F), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(cameraLocation, glm::vec3(0.0F, -5.0F, 0.0F), floatEpsilon)));

                // Move node.
                pChildCameraNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                targetLocation = pCameraProperties->getOrbitalCameraTargetLocation(true);
                cameraLocation = pCameraProperties->getLocation(true);
                REQUIRE(
                    glm::all(glm::epsilonEqual(targetLocation, glm::vec3(0.0F, 10.0F, 0.0F), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(cameraLocation, glm::vec3(0.0F, 0.0F, 0.0F), floatEpsilon)));

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
