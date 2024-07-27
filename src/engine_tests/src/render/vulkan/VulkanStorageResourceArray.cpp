// Custom.
#include "render/vulkan/resources/VulkanStorageResourceArray.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "render/vulkan/resources/VulkanStorageResourceArrayManager.h"
#include "game/nodes/MeshNode.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "render/general/resources/frame/FrameResourceManager.h"

// External.
#include "catch2/catch_test_macros.hpp"

static constexpr auto pTargetShaderResourceName = "meshData";

TEST_CASE("make array expand / shrink") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}

        virtual void onGameStarted() override {
            // Make sure we are using Vulkan renderer.
            if (dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-Vulkan renderer.
                getWindow()->close();
                SKIP();
            }

            // Create world.
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) [[unlikely]] {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Get resource manager.
                const auto pVulkanResourceManager =
                    dynamic_cast<VulkanResourceManager*>(getWindow()->getRenderer()->getResourceManager());
                REQUIRE(pVulkanResourceManager != nullptr);

                // Get storage array manager.
                const auto pArrayManager = pVulkanResourceManager->getStorageResourceArrayManager();
                REQUIRE(pArrayManager != nullptr);

                // Spawn sample mesh.
                pMeshNode = sgc::makeGc<MeshNode>();
                pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pMeshNode);

                // Make sure storage array now has 1 item.
                const auto pMeshDataArray =
                    pArrayManager->getArrayForShaderResource(pTargetShaderResourceName);
                REQUIRE(pMeshDataArray != nullptr);
                REQUIRE(pMeshDataArray->getSize() == FrameResourceManager::getFrameResourceCount());

                // Save initial capacity.
                const auto iInitialCapacity = pMeshDataArray->getCapacity();
                REQUIRE(iInitialCapacity == pMeshDataArray->getCapacityStepSize());

                // Spawn some temp more nodes.
                sgc::GcVector<sgc::GcPtr<MeshNode>> vTempNodes;
                const auto iTempNodeCount = 2;
                for (size_t i = 0; i < iTempNodeCount; i++) {
                    // Spawn sample mesh.
                    const auto pMeshNode = sgc::makeGc<MeshNode>();
                    pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                    getWorldRootNode()->addChildNode(pMeshNode);
                    vTempNodes.push_back(pMeshNode);
                }

                // Make sure there are no unused slots.
                const auto pMtxMeshArrayResources = pMeshDataArray->getInternalResources();
                {
                    std::scoped_lock guard(pMtxMeshArrayResources->first);
                    REQUIRE(
                        pMtxMeshArrayResources->second.iNextFreeArrayIndex ==
                        (iTempNodeCount + 1) * FrameResourceManager::getFrameResourceCount());
                    REQUIRE(pMtxMeshArrayResources->second.noLongerUsedArrayIndices.empty());
                }

                // Despawn temp nodes.
                for (size_t i = 0; i < iTempNodeCount; i++) {
                    vTempNodes.back()->detachFromParentAndDespawn();
                    vTempNodes.pop_back();
                }

                // Make sure there are unused slots.
                {
                    std::scoped_lock guard(pMtxMeshArrayResources->first);
                    REQUIRE(
                        pMtxMeshArrayResources->second.iNextFreeArrayIndex ==
                        (iTempNodeCount + 1) * FrameResourceManager::getFrameResourceCount());
                    REQUIRE(
                        pMtxMeshArrayResources->second.noLongerUsedArrayIndices.size() ==
                        iTempNodeCount * FrameResourceManager::getFrameResourceCount());
                }

                // Add more mesh nodes to make the array expand.
                const auto iExpectedCapacitySizeMult = 3;
                const auto iMeshToSpawnCount = pMeshDataArray->getCapacityStepSize() *
                                               iExpectedCapacitySizeMult /
                                               FrameResourceManager::getFrameResourceCount();
                for (size_t i = 0; i < iMeshToSpawnCount; i++) {
                    // Spawn sample mesh.
                    const auto pMeshNode = sgc::makeGc<MeshNode>();
                    pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                    getWorldRootNode()->addChildNode(pMeshNode);
                    vMeshNodes.push_back(pMeshNode);
                }

                // Now array has expanded.
                REQUIRE(
                    pMeshDataArray->getSize() ==
                    (iMeshToSpawnCount + 1) * FrameResourceManager::getFrameResourceCount());
                REQUIRE(pMeshDataArray->getCapacity() > iInitialCapacity);
                REQUIRE(
                    pMeshDataArray->getCapacity() ==
                    pMeshDataArray->getCapacityStepSize() * (iExpectedCapacitySizeMult + 1));

                // Make sure there are no unused slots.
                {
                    std::scoped_lock guard(pMtxMeshArrayResources->first);
                    REQUIRE(pMtxMeshArrayResources->second.iNextFreeArrayIndex == pMeshDataArray->getSize());
                    REQUIRE(pMtxMeshArrayResources->second.noLongerUsedArrayIndices.empty());
                }

                // Now make the array shrink.
                const auto iMeshCountToDespawn =
                    pMeshDataArray->getCapacityStepSize() / FrameResourceManager::getFrameResourceCount();
                REQUIRE(vMeshNodes.size() > iMeshCountToDespawn);
                auto iMeshNodeCount = vMeshNodes.size() + 1;
                for (size_t i = 0; i < iMeshCountToDespawn; i++) {
                    vMeshNodes.back()->detachFromParentAndDespawn();
                    vMeshNodes.pop_back();
                }
                iMeshNodeCount = vMeshNodes.size() + 1;

                // Now array has shrinked.
                REQUIRE(
                    pMeshDataArray->getSize() ==
                    (vMeshNodes.size() + 1) * FrameResourceManager::getFrameResourceCount());
                REQUIRE(pMeshDataArray->getCapacity() > iInitialCapacity);
                REQUIRE(
                    pMeshDataArray->getCapacity() ==
                    pMeshDataArray->getCapacityStepSize() * iExpectedCapacitySizeMult);
                REQUIRE(
                    iMeshNodeCount * FrameResourceManager::getFrameResourceCount() ==
                    pMeshDataArray->getSize());

                // Make sure there are unused slots.
                {
                    std::scoped_lock guard(pMtxMeshArrayResources->first);
                    REQUIRE(pMtxMeshArrayResources->second.iNextFreeArrayIndex > pMeshDataArray->getSize());
                    REQUIRE(!pMtxMeshArrayResources->second.noLongerUsedArrayIndices.empty());
                }

                // Now wait for a few frames to be drawn so that validation layers will log an error
                // if descriptors were not updated after array resize.
                iFramesPassed = 0;
            });
        }

        virtual void onBeforeNewFrame(float timeSincePrevCallInSec) override {
            iFramesPassed += 1;

            if (iFramesPassed > iFramesToWait) {
                // Get resource manager.
                const auto pVulkanResourceManager =
                    dynamic_cast<VulkanResourceManager*>(getWindow()->getRenderer()->getResourceManager());
                REQUIRE(pVulkanResourceManager != nullptr);

                // Get storage array manager.
                const auto pArrayManager = pVulkanResourceManager->getStorageResourceArrayManager();
                REQUIRE(pArrayManager != nullptr);

                const auto pMeshDataArray =
                    pArrayManager->getArrayForShaderResource(pTargetShaderResourceName);

                // Despawn all left mesh nodes.
                const auto iNodesLeft = vMeshNodes.size() + 1;
                REQUIRE(
                    iNodesLeft * FrameResourceManager::getFrameResourceCount() == pMeshDataArray->getSize());

                // Despawn nodes.
                const auto iNodesToDespawnCount = vMeshNodes.size();
                for (size_t i = 0; i < iNodesToDespawnCount; i++) {
                    vMeshNodes.back()->detachFromParentAndDespawn();
                    vMeshNodes.pop_back();
                }
                pMeshNode->detachFromParentAndDespawn();

                // Make sure storage array now has 0 items.
                REQUIRE(pMeshDataArray != nullptr);
                REQUIRE(pMeshDataArray->getSize() == 0);
                REQUIRE(pMeshDataArray->getCapacity() == pMeshDataArray->getCapacityStepSize());

                getWindow()->close();
            }
        }

        virtual ~TestGameInstance() override {}

    private:
        sgc::GcVector<sgc::GcPtr<MeshNode>> vMeshNodes;
        sgc::GcPtr<MeshNode> pMeshNode;
        size_t iFramesPassed = 0;
        const size_t iFramesToWait = 10;
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
}
