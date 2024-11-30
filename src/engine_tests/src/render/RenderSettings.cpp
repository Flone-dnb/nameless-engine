// Custom.
#include "game/node/MeshNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "game/camera/CameraManager.h"
#include "render/RenderSettings.h"
#include "game/node/CameraNode.h"
#include "TestHelpers.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("change resolution/msaa/texture filtering/vsync while MeshNode is spawned then restart game and "
          "check saved settings") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    private:
        std::pair<unsigned int, unsigned int> usedResolution;
        AntialiasingQuality usedMsaa;
        bool bIsVsyncEnabled = false;
        TextureFilteringQuality textureFilteringQuality;

        size_t iTickCount = 0;
        bool bChangedResolution = false;
        bool bChangesMsaa = false;
        bool bChangedVsync = false;
        bool bChangedTextureFiltering = false;

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

                // Prepare textures(because we will change texture filtering).
                auto importResult = TestHelpers::prepareDiffuseTextures();
                if (std::holds_alternative<Error>(importResult)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(importResult));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                const auto vImportedTexturePaths =
                    std::get<std::array<std::string, 2>>(std::move(importResult));

                // Create camera.
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

                // Create node and initialize.
                const auto pMeshNode = sgc::makeGc<MeshNode>();
                pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));

                // Set texture.
                pMeshNode->getMaterial()->setDiffuseTexture(vImportedTexturePaths[0]);

                // Spawn mesh node.
                getWorldRootNode()->addChildNode(pMeshNode);
            });
        }

        virtual void onBeforeNewFrame(float timeSincePrevCallInSec) override {
            iTickCount += 1;

            if (iTickCount > 1) {
                REQUIRE(getTotalSpawnedNodeCount() == 3);
            }

            if (iTickCount == 3) {
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

                const auto mtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

                usedResolution = mtxRenderSettings.second->getRenderResolution();

                // Get some supported resolution.
                const auto targetResolution = *supportedResolutions.begin();

                if (getWindow()->getRenderer()->getType() != RendererType::VULKAN) {
                    // Make sure it's not the same.
                    // We still haven't implemented window-independent render resolution for Vulkan
                    // the it will only have 1 available render resolution.
                    REQUIRE(usedResolution != targetResolution);
                }

                // Change resolution.
                mtxRenderSettings.second->setRenderResolution(targetResolution);

                // Done.
                bChangedResolution = true;
            } else if (iTickCount == 5) {
                // Get settings.
                const auto mtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

                mtxRenderSettings.second->setRenderResolution(usedResolution);

                const auto antialiasingQuality = mtxRenderSettings.second->getAntialiasingQuality();
                REQUIRE(antialiasingQuality.has_value());
                usedMsaa = *antialiasingQuality;

                // Change MSAA.
                if (usedMsaa != AntialiasingQuality::DISABLED) {
                    if (usedMsaa == AntialiasingQuality::HIGH) {
                        mtxRenderSettings.second->setAntialiasingQuality(AntialiasingQuality::MEDIUM);
                    } else {
                        mtxRenderSettings.second->setAntialiasingQuality(AntialiasingQuality::HIGH);
                    }
                } else {
                    mtxRenderSettings.second->setAntialiasingQuality(AntialiasingQuality::MEDIUM);
                }

                // Done.
                bChangesMsaa = true;
            } else if (iTickCount == 7) {
                // Get settings.
                const auto mtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

                // Restore MSAA.
                mtxRenderSettings.second->setAntialiasingQuality(usedMsaa);

                // Change VSync.
                bIsVsyncEnabled = mtxRenderSettings.second->isVsyncEnabled();
                mtxRenderSettings.second->setVsyncEnabled(!bIsVsyncEnabled);

                bChangedVsync = true;
            } else if (iTickCount == 9) {
                // Get settings.
                const auto mtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

                // Restore VSync.
                mtxRenderSettings.second->setVsyncEnabled(bIsVsyncEnabled);

                textureFilteringQuality = mtxRenderSettings.second->getTextureFilteringQuality();

                // Change texture filtering.
                if (textureFilteringQuality == TextureFilteringQuality::HIGH) {
                    mtxRenderSettings.second->setTextureFilteringQuality(TextureFilteringQuality::LOW);
                } else {
                    mtxRenderSettings.second->setTextureFilteringQuality(TextureFilteringQuality::HIGH);
                }

                bChangedTextureFiltering = true;
            } else if (iTickCount == 11) {
                // Get settings.
                const auto mtxRenderSettings = getWindow()->getRenderer()->getRenderSettings();
                std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

                // Restore texture filtering.
                mtxRenderSettings.second->setTextureFilteringQuality(textureFilteringQuality);

                getWindow()->close();
            }
        }
        virtual void onWindowClose() override {
            REQUIRE(bChangedResolution);
            REQUIRE(bChangesMsaa);
            REQUIRE(bChangedVsync);
            REQUIRE(bChangedTextureFiltering);
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
