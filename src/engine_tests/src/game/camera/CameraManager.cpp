// Custom.
#include "game/node/CameraNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/camera/CameraManager.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("make camera node to be the active camera") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn camera node.
                const auto pCameraNode = sgc::makeGc<CameraNode>();
                getWorldRootNode()->addChildNode(pCameraNode);

                // Make camera node to be the active one.
                getCameraManager()->setActiveCamera(pCameraNode);

                // Make sure it's the active one.
                {
                    const auto pMtxActiveCamera = getCameraManager()->getActiveCamera();
                    std::scoped_lock guard(pMtxActiveCamera->first);
                    REQUIRE(pMtxActiveCamera->second == pCameraNode);
                }

                // Check that no warnings/errors will be logged.
                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
