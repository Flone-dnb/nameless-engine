// Custom.
#include "game/nodes/MeshNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "game/camera/CameraManager.h"
#include "render/RenderSettings.h"
#include "game/nodes/CameraNode.h"
#include "TestHelpers.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE(
    "change resolution/msaa/vsync while MeshNode is spawned then restart game and check saved settings") {
    using namespace ne;

    static std::pair<unsigned int, unsigned int> targetResolution;
    static bool bIsAaEnabled = false;
    static bool bIsVsyncEnabled = false;

    class TestGameInstance : public GameInstance {
    private:
        size_t iTickCount = 0;
        bool bChangedResolution = false;
        bool bChangesMsaa = false;
        bool bChangedVsync = false;

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

                // Create camera.
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

                // Create node and initialize.
                const auto pMeshNode = gc_new<MeshNode>();
                pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));

                // Spawn mesh node.
                getWorldRootNode()->addChildNode(pMeshNode);
            });
        }
        virtual void onBeforeNewFrame(float timeSincePrevCallInSec) override {
            iTickCount += 1;

            if (iTickCount > 1) {
                REQUIRE(getTotalSpawnedNodeCount() == 3);
            }

            if (iTickCount == 2) {
                // Change resolution.

                // Get supported resolutions.
                auto result = getWindow()->getRenderer()->getSupportedRenderResolutions();
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                const auto supportedResolutions =
                    std::get<std::set<std::pair<unsigned int, unsigned int>>>(std::move(result));
                REQUIRE(!supportedResolutions.empty());

                // Get some supported resolution.
                targetResolution = *supportedResolutions.begin();

                const auto pMtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);

                // Make sure it's not the same.
                REQUIRE(pMtxRenderSettings->second->getRenderResolution() != targetResolution);

                // Apply.
                pMtxRenderSettings->second->setRenderResolution(targetResolution);

                // Done.
                bChangedResolution = true;
            } else if (iTickCount == 3) {
                // Change MSAA.

                // Get settings.
                const auto pMtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);

                // Apply.
                bIsAaEnabled = pMtxRenderSettings->second->getAntialiasingQuality() != AntialiasingQuality::DISABLED;
                if (bIsAaEnabled) {
                    if (pMtxRenderSettings->second->getAntialiasingQuality() == AntialiasingQuality::HIGH) {
                        pMtxRenderSettings->second->setAntialiasingQuality(AntialiasingQuality::MEDIUM);
                    } else {
                        pMtxRenderSettings->second->setAntialiasingQuality(AntialiasingQuality::HIGH);
                    }
                } else {
                    pMtxRenderSettings->second->setAntialiasingQuality(AntialiasingQuality::MEDIUM);
                }

                // Done.
                bChangesMsaa = true;
            } else if (iTickCount == 4) {
                // Change VSync.

                // Get settings.
                const auto pMtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);

                // Apply.
                bIsVsyncEnabled = !pMtxRenderSettings->second->isVsyncEnabled();
                pMtxRenderSettings->second->setVsyncEnabled(bIsVsyncEnabled);

                bChangedVsync = true;
            } else if (iTickCount == 5) {
                getWindow()->close();
            }
        }
        virtual void onWindowClose() override {
            REQUIRE(bChangedResolution);
            REQUIRE(bChangesMsaa);
            REQUIRE(bChangedVsync);
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

    class TestGameInstance2 : public GameInstance {
    public:
        TestGameInstance2(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            const auto pMtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
            std::scoped_lock guard(pMtxRenderSettings->first);

            REQUIRE(pMtxRenderSettings->second->getRenderResolution() == targetResolution);
            if (bIsAaEnabled) {
                REQUIRE(pMtxRenderSettings->second->getAntialiasingQuality() != AntialiasingQuality::DISABLED);
            } else {
                REQUIRE(pMtxRenderSettings->second->getAntialiasingQuality() == AntialiasingQuality::DISABLED);
            }
            REQUIRE(pMtxRenderSettings->second->isVsyncEnabled() == bIsVsyncEnabled);

            getWindow()->close();
        }
    };

    pMainWindow->processEvents<TestGameInstance2>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
