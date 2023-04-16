// Custom.
#include "game/nodes/CameraNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/Globals.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("orbital camera node behaves correctly when used in a node tree") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
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

                // Spawn in world.
                pParentSpatialNode->addChildNode(pChildCameraNode);
                getWorldRootNode()->addChildNode(pParentSpatialNode);

                // Set mode.
                pChildCameraNode->setCameraMode(CameraMode::ORBITAL);
                auto orbitalProperties = pChildCameraNode->getCameraProperties()->getOrbitalModeProperties();

                // Check.
                REQUIRE(glm::epsilonEqual(orbitalProperties.phi, 0.0F, floatEpsilon));
                REQUIRE(glm::epsilonEqual(orbitalProperties.theta, 0.0F, floatEpsilon));

                // Set relative location.
                pChildCameraNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                auto cameraLocation = pChildCameraNode->getWorldLocation();
                auto cameraRelativeLocation = pChildCameraNode->getRelativeLocation();
                const auto cameraForward = pChildCameraNode->getWorldForwardDirection();
                orbitalProperties = pChildCameraNode->getCameraProperties()->getOrbitalModeProperties();

                // Check.
                REQUIRE(glm::epsilonEqual(orbitalProperties.phi, 90.0F, floatEpsilon));
                REQUIRE(glm::epsilonEqual(orbitalProperties.theta, 90.0F, floatEpsilon));
                REQUIRE(glm::epsilonEqual(orbitalProperties.distanceToTarget, 5.0F, floatEpsilon));
                REQUIRE(
                    glm::all(glm::epsilonEqual(cameraLocation, glm::vec3(0.0F, 5.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(cameraRelativeLocation, glm::vec3(5.0F, 0.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(cameraForward, -Globals::WorldDirection::right, floatEpsilon)));

                // Set distance to target.
                pChildCameraNode->setOrbitalDistanceToTarget(10.0F); // causes location to change

                cameraLocation = pChildCameraNode->getWorldLocation();
                cameraRelativeLocation = pChildCameraNode->getRelativeLocation();
                orbitalProperties = pChildCameraNode->getCameraProperties()->getOrbitalModeProperties();

                // Check.
                REQUIRE(
                    glm::all(glm::epsilonEqual(cameraLocation, glm::vec3(0.0F, 10.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(cameraRelativeLocation, glm::vec3(10.0F, 0.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::epsilonEqual(orbitalProperties.phi, 90.0F, floatEpsilon));
                REQUIRE(glm::epsilonEqual(orbitalProperties.theta, 90.0F, floatEpsilon));
                REQUIRE(glm::epsilonEqual(orbitalProperties.distanceToTarget, 10.0F, floatEpsilon));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildCameraNode->getOrbitalTargetLocation(),
                    glm::vec3(0.0F, 0.0F, 0.0F),
                    floatEpsilon)));

                // Move parent.
                pParentSpatialNode->setRelativeLocation(glm::vec3(0.0F, 5.0F, 0.0F));

                orbitalProperties = pChildCameraNode->getCameraProperties()->getOrbitalModeProperties();
                cameraLocation = pChildCameraNode->getWorldLocation();
                cameraRelativeLocation = pChildCameraNode->getRelativeLocation();

                // Check.
                REQUIRE(
                    glm::all(glm::epsilonEqual(cameraLocation, glm::vec3(0.0F, 15.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(cameraRelativeLocation, glm::vec3(10.0F, 0.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::epsilonEqual(orbitalProperties.distanceToTarget, 10.0F, floatEpsilon));
                REQUIRE(glm::epsilonEqual(orbitalProperties.phi, 90.0F, floatEpsilon));
                REQUIRE(glm::epsilonEqual(orbitalProperties.theta, 90.0F, floatEpsilon));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildCameraNode->getOrbitalTargetLocation(),
                    glm::vec3(0.0F, 5.0F, 0.0F),
                    floatEpsilon)));

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
