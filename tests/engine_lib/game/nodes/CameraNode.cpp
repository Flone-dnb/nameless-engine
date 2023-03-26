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

                constexpr float floatEpsilon = 0.00001f;

                const auto pParentSpatialNode = gc_new<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0F, 0.0F, 90.0F));

                const auto pChildCameraNode = gc_new<CameraNode>();
                const auto pCameraProperties = pChildCameraNode->getCameraProperties();
                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getForwardDirection(true), worldForwardDirection, floatEpsilon)));
                pChildCameraNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                // Spawn in world.
                pParentSpatialNode->addChildNode(pChildCameraNode);
                getWorldRootNode()->addChildNode(pParentSpatialNode);

                // Compare final camera data.
                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getLocation(true), glm::vec3(5.0F, -5.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getForwardDirection(true), worldRightDirection, floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pCameraProperties->getRightDirection(true), -worldForwardDirection, floatEpsilon)));

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
