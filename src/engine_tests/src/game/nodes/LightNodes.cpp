// Custom.
#include "game/nodes/light/DirectionalLightNode.h"
#include "game/nodes/light/PointLightNode.h"
#include "game/nodes/light/SpotlightNode.h"
#include "game/camera/TransientCamera.h"
#include "game/camera/CameraManager.h"
#include "game/nodes/MeshNode.h"
#include "game/GameInstance.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("spawn light sources and then a mesh") {
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

                // Create and setup camera.
                auto pCamera = std::make_shared<TransientCamera>();
                pCamera->setLocation(glm::vec3(-2.0F, 0.0F, 0.0F));

                // Make it active.
                getCameraManager()->setActiveCamera(pCamera);

                // Spawn some light sources.
                getWorldRootNode()->addChildNode(gc_new<DirectionalLightNode>());
                getWorldRootNode()->addChildNode(gc_new<PointLightNode>());
                getWorldRootNode()->addChildNode(gc_new<SpotlightNode>());

                // Now spawn a mesh.
                getWorldRootNode()->addChildNode(gc_new<MeshNode>());

                iFrameCount = 0;
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            // Wait a few frames to check for log errors.
            if (iFrameCount == 2) {
                // Make sure something was rendered (in case we forgot the camera).
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() > 0);

                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
        gc<MeshNode> pMeshNode;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) [[unlikely]] {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("spawn mesh and then light sources") {
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

                // Create and setup camera.
                auto pCamera = std::make_shared<TransientCamera>();
                pCamera->setLocation(glm::vec3(-2.0F, 0.0F, 0.0F));

                // Make it active.
                getCameraManager()->setActiveCamera(pCamera);

                // Spawn a mesh.
                getWorldRootNode()->addChildNode(gc_new<MeshNode>());

                // Now spawn some light sources.
                getWorldRootNode()->addChildNode(gc_new<DirectionalLightNode>());
                getWorldRootNode()->addChildNode(gc_new<PointLightNode>());
                getWorldRootNode()->addChildNode(gc_new<SpotlightNode>());

                iFrameCount = 0;
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            // Wait a few frames to check for log errors.
            if (iFrameCount == 2) {
                // Make sure something was rendered (in case we forgot the camera).
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() > 0);

                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
        gc<MeshNode> pMeshNode;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) [[unlikely]] {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("change render settings with lights spawned") {
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

                // Create and setup camera.
                auto pCamera = std::make_shared<TransientCamera>();
                pCamera->setLocation(glm::vec3(-2.0F, 0.0F, 0.0F));

                // Make it active.
                getCameraManager()->setActiveCamera(pCamera);

                // Spawn a mesh.
                const auto pFloorMesh = gc_new<MeshNode>();
                pFloorMesh->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                pFloorMesh->setRelativeLocation(glm::vec3(0.0F, 0.0F, -5.0F));
                pFloorMesh->setRelativeScale(glm::vec3(100.0F, 100.0F, 1.0F));
                getWorldRootNode()->addChildNode(pFloorMesh);

                // Now spawn some light sources.
                getWorldRootNode()->addChildNode(gc_new<DirectionalLightNode>());
                getWorldRootNode()->addChildNode(gc_new<PointLightNode>());
                getWorldRootNode()->addChildNode(gc_new<SpotlightNode>());

                iFrameCount = 0;
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            if (iFrameCount == 2) {
                // Make sure something was rendered (in case we forgot the camera).
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() > 0);

                // Get render settings.
                const auto pMtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock guard(pMtxRenderSettings->first);

                // Change MSAA.
                const auto msaaState = pMtxRenderSettings->second->getAntialiasingState();
                if (msaaState != MsaaState::HIGH) {
                    pMtxRenderSettings->second->setAntialiasingState(MsaaState::HIGH);
                } else {
                    pMtxRenderSettings->second->setAntialiasingState(MsaaState::MEDIUM);
                }

                return;
            }

            if (iFrameCount == 4) {
                // Make sure something was rendered (in case we forgot the camera).
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() > 0);

                // Get render settings.
                const auto pMtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock guard(pMtxRenderSettings->first);

                // Change shadow quality.
                const auto shadowQuality = pMtxRenderSettings->second->getShadowQuality();
                if (shadowQuality != ShadowQuality::HIGH) {
                    pMtxRenderSettings->second->setShadowQuality(ShadowQuality::HIGH);
                } else {
                    pMtxRenderSettings->second->setShadowQuality(ShadowQuality::MEDIUM);
                }

                // Wait a few frames to check for log errors.
                return;
            }

            if (iFrameCount == 6) {
                // Make sure something was rendered (in case we forgot the camera).
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() > 0);

                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
        gc<MeshNode> pMeshNode;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) [[unlikely]] {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
