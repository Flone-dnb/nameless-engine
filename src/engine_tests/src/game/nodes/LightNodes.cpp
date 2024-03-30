// Custom.
#include "game/nodes/light/DirectionalLightNode.h"
#include "game/nodes/light/PointLightNode.h"
#include "game/nodes/light/SpotlightNode.h"
#include "game/nodes/CameraNode.h"
#include "TestHelpers.hpp"
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
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-2.0F, 0.0F, 0.0F));

                // Spawn some light sources.
                getWorldRootNode()->addChildNode(sgc::makeGc<DirectionalLightNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<PointLightNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<SpotlightNode>());

                // Now spawn a mesh.
                getWorldRootNode()->addChildNode(sgc::makeGc<MeshNode>());

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
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-2.0F, 0.0F, 0.0F));

                // Spawn a mesh.
                getWorldRootNode()->addChildNode(sgc::makeGc<MeshNode>());

                // Now spawn some light sources.
                getWorldRootNode()->addChildNode(sgc::makeGc<DirectionalLightNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<PointLightNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<SpotlightNode>());

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
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-2.0F, 0.0F, 0.0F));

                // Spawn a mesh.
                const auto pFloorMesh = sgc::makeGc<MeshNode>();
                pFloorMesh->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                pFloorMesh->setRelativeLocation(glm::vec3(0.0F, 0.0F, -5.0F));
                pFloorMesh->setRelativeScale(glm::vec3(100.0F, 100.0F, 1.0F));
                getWorldRootNode()->addChildNode(pFloorMesh);

                // Now spawn some light sources.
                getWorldRootNode()->addChildNode(sgc::makeGc<DirectionalLightNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<PointLightNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<SpotlightNode>());

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
                const auto msaaState = pMtxRenderSettings->second->getAntialiasingQuality();
                if (msaaState != AntialiasingQuality::HIGH) {
                    pMtxRenderSettings->second->setAntialiasingQuality(AntialiasingQuality::HIGH);
                } else {
                    pMtxRenderSettings->second->setAntialiasingQuality(AntialiasingQuality::MEDIUM);
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

TEST_CASE("point light culled when outside of camera frustum") {
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
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-20.0F, 0.0F, 0.0F));

                // Spawn a floor mesh.
                const auto pFloorMesh = sgc::makeGc<MeshNode>();
                pFloorMesh->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                pFloorMesh->setRelativeLocation(glm::vec3(0.0F, 0.0F, -5.0F));
                pFloorMesh->setRelativeScale(glm::vec3(100.0F, 100.0F, 1.0F));
                getWorldRootNode()->addChildNode(pFloorMesh);

                // Now spawn light.
                const auto pPointLight = sgc::makeGc<PointLightNode>();
                pPointLight->setLightDistance(10.0F);
                pPointLight->setRelativeLocation(glm::vec3(0.0F, 0.0F, 0.0F));
                getWorldRootNode()->addChildNode(pPointLight);

                iFrameCount = 0;
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            // Get render stats.
            const auto pRenderStats = getWindow()->getRenderer()->getRenderStatistics();

            if (iFrameCount == 2) {
                // No lights should be culled.
                REQUIRE(pRenderStats->getLastFrameCulledLightCount() == 0);

                // Check draw calls:
                // 6 in shadow pass (no culling there yet) + 1 in depth prepass + 1 in main pass
                REQUIRE(pRenderStats->getLastFrameDrawCallCount() == 8);

                // Rotate the camera 180 degrees.
                getCameraManager()->getActiveCamera()->second->setRelativeRotation(
                    glm::vec3(0.0F, 0.0F, 180.0F));

                return;
            }

            if (iFrameCount == 3) {
                // Point light should have been culled.
                REQUIRE(pRenderStats->getLastFrameCulledLightCount() == 1);

                // Check draw calls:
                // 1 in depth prepass + 1 in main pass
                REQUIRE(pRenderStats->getLastFrameDrawCallCount() == 2);

                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
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

TEST_CASE("spotlight culled when outside of camera frustum") {
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
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-20.0F, 0.0F, 0.0F));

                // Spawn a floor mesh.
                const auto pFloorMesh = sgc::makeGc<MeshNode>();
                pFloorMesh->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                pFloorMesh->setRelativeLocation(glm::vec3(0.0F, 0.0F, -5.0F));
                pFloorMesh->setRelativeScale(glm::vec3(100.0F, 100.0F, 1.0F));
                getWorldRootNode()->addChildNode(pFloorMesh);

                // Now spawn light.
                const auto pSpotlight = sgc::makeGc<SpotlightNode>();
                pSpotlight->setLightDistance(10.0F);
                pSpotlight->setRelativeLocation(glm::vec3(0.0F, 0.0F, 0.0F));
                getWorldRootNode()->addChildNode(pSpotlight);

                iFrameCount = 0;
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            // Get render stats.
            const auto pRenderStats = getWindow()->getRenderer()->getRenderStatistics();

            if (iFrameCount == 2) {
                // No lights should be culled.
                REQUIRE(pRenderStats->getLastFrameCulledLightCount() == 0);

                // Check draw calls:
                // 1 in shadow pass + 1 in depth prepass + 1 in main pass
                REQUIRE(pRenderStats->getLastFrameDrawCallCount() == 3);

                // Rotate the camera 180 degrees.
                getCameraManager()->getActiveCamera()->second->setRelativeRotation(
                    glm::vec3(0.0F, 0.0F, 180.0F));

                return;
            }

            if (iFrameCount == 3) {
                // Spotlight should have been culled.
                REQUIRE(pRenderStats->getLastFrameCulledLightCount() == 1);

                // Check draw calls:
                // 1 in depth prepass + 1 in main pass
                REQUIRE(pRenderStats->getLastFrameDrawCallCount() == 2);

                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
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
