#include "EditorGameInstance.h"

// Standard.
#include <format>

// Custom.
#include "game/camera/CameraManager.h"
#include "game/nodes/MeshNode.h"
#include "game/Window.h"
#include "input/InputManager.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "game/nodes/EnvironmentNode.h"
#include "game/nodes/light/DirectionalLightNode.h"
#include "game/nodes/light/PointLightNode.h"
#include "math/MathHelpers.hpp"
#include "game/nodes/light/SpotlightNode.h"
#include "misc/EditorNodeCreationHelpers.hpp"
#include "nodes/EditorCameraNode.h"
#include "input/EditorInputEventIds.hpp"
#include "render/general/resources/GpuResourceManager.h"

namespace ne {
    const char* EditorGameInstance::getEditorWindowTitle() { return pEditorWindowTitle; }

    EditorGameInstance::EditorGameInstance(
        Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
        : GameInstance(pWindow, pGameManager, pInputManager) {
        // Move forward.
        auto optionalError = pInputManager->addAxisEvent(
            static_cast<unsigned int>(EditorInputEventIds::Axis::MOVE_CAMERA_FORWARD),
            {std::make_pair(KeyboardKey::KEY_W, KeyboardKey::KEY_S)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Move right.
        optionalError = pInputManager->addAxisEvent(
            static_cast<unsigned int>(EditorInputEventIds::Axis::MOVE_CAMERA_RIGHT),
            {std::make_pair(KeyboardKey::KEY_D, KeyboardKey::KEY_A)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Move up.
        optionalError = pInputManager->addAxisEvent(
            static_cast<unsigned int>(EditorInputEventIds::Axis::MOVE_CAMERA_UP),
            {std::make_pair(KeyboardKey::KEY_E, KeyboardKey::KEY_Q)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Capture mouse.
        optionalError = pInputManager->addActionEvent(
            static_cast<unsigned int>(EditorInputEventIds::Action::CAPTURE_MOUSE_CURSOR),
            {MouseButton::RIGHT});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Increase camera speed.
        optionalError = pInputManager->addActionEvent(
            static_cast<unsigned int>(EditorInputEventIds::Action::INCREASE_CAMERA_SPEED),
            {KeyboardKey::KEY_LEFT_SHIFT});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Decrease camera speed.
        optionalError = pInputManager->addActionEvent(
            static_cast<unsigned int>(EditorInputEventIds::Action::DECREASE_CAMERA_SPEED),
            {KeyboardKey::KEY_LEFT_CONTROL});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Bind action events.
        {
            const auto pMtxActionEventBindings = getActionEventBindings();
            std::scoped_lock guard(pMtxActionEventBindings->first);

            pMtxActionEventBindings
                ->second[static_cast<unsigned int>(EditorInputEventIds::Action::CAPTURE_MOUSE_CURSOR)] =
                [this](KeyboardModifiers modifiers, bool bIsPressed) {
                    if (gcPointers.pCameraNode == nullptr) {
                        return;
                    }

                    // Set cursor visibility.
                    getWindow()->setCursorVisibility(!bIsPressed);

                    // Notify editor camera.
                    gcPointers.pCameraNode->setIgnoreInput(!bIsPressed);
                };
        }
    }

    sgc::GcPtr<EditorCameraNode> EditorGameInstance::getEditorCamera() const {
        return gcPointers.pCameraNode;
    }

    void EditorGameInstance::onGameStarted() {
        // Create world.
        createWorld([this](const std::optional<Error>& optionalWorldError) {
            if (optionalWorldError.has_value()) [[unlikely]] {
                auto error = optionalWorldError.value();
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Spawn editor nodes.
            spawnEditorNodesForNewWorld();

            // Spawn environment node.
            const auto pEnvironmentNode = sgc::makeGc<EnvironmentNode>();
            pEnvironmentNode->setAmbientLight(glm::vec3(0.1F, 0.1F, 0.1F)); // NOLINT
            getWorldRootNode()->addChildNode(pEnvironmentNode);

            // Spawn directional light.
            const auto pDirectionalLightNode = sgc::makeGc<DirectionalLightNode>();
            getWorldRootNode()->addChildNode(pDirectionalLightNode);
            pDirectionalLightNode->setWorldRotation(MathHelpers::convertDirectionToRollPitchYaw(
                glm::normalize(glm::vec3(1.0F, -0.5F, -1.0F)))); // NOLINT
            pDirectionalLightNode->setLightIntensity(0.1F);      // NOLINT
            pDirectionalLightNode->setLightColor(glm::vec3(0.0F, 0.0F, 1.0F));

            // Spawn point light.
            const auto pPointLightNode = sgc::makeGc<PointLightNode>();
            getWorldRootNode()->addChildNode(pPointLightNode);
            pPointLightNode->setLightColor(glm::vec3(1.0F, 0.0F, 0.0F));
            pPointLightNode->setWorldLocation(glm::vec3(3.0F, 4.0F, 2.5F)); // NOLINT
            pPointLightNode->setLightDistance(15.0F);                       // NOLINT

            // Spawn floor.
            const auto pFloorNode = sgc::makeGc<MeshNode>();
            pFloorNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
            getWorldRootNode()->addChildNode(pFloorNode);
            pFloorNode->setWorldScale(glm::vec3(100.0F, 100.0F, 1.0F)); // NOLINT
            pFloorNode->getMaterial()->setRoughness(0.8F);              // NOLINT

            // Spawn spotlight.
            const auto pSpotlightNode = sgc::makeGc<SpotlightNode>();
            getWorldRootNode()->addChildNode(pSpotlightNode);
            pSpotlightNode->setLightColor(glm::vec3(0.0F, 1.0F, 0.0F));
            pSpotlightNode->setWorldLocation(glm::vec3(12.0F, 4.0F, 2.5F)); // NOLINT
            pSpotlightNode->setWorldRotation(MathHelpers::convertDirectionToRollPitchYaw(
                glm::normalize(glm::vec3(-0.5F, -1.0F, -1.0F)))); // NOLINT
            pSpotlightNode->setLightInnerConeAngle(10.0F);        // NOLINT
            pSpotlightNode->setLightInnerConeAngle(20.0F);        // NOLINT
            pSpotlightNode->setLightDistance(15.0F);              // NOLINT

            {
                // Spawn cube 1.
                const auto pCubeNode = sgc::makeGc<MeshNode>();
                pCubeNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pCubeNode);
                pCubeNode->setWorldLocation(glm::vec3(0.0F, 0.0F, 1.0F)); // NOLINT
                pCubeNode->setWorldScale(glm::vec3(3.0F, 3.0F, 1.0F));    // NOLINT
            }
            {
                // Spawn cube 2.
                const auto pCubeNode = sgc::makeGc<MeshNode>();
                pCubeNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pCubeNode);
                pCubeNode->setWorldLocation(glm::vec3(10.0F, 0.0F, 1.0F)); // NOLINT
                pCubeNode->setWorldScale(glm::vec3(1.0F, 1.0F, 3.0F));     // NOLINT
            }
            {
                // Spawn cube 3.
                const auto pCubeNode = sgc::makeGc<MeshNode>();
                pCubeNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pCubeNode);
                pCubeNode->setWorldLocation(glm::vec3(3.0F, -1.0F, 4.0F)); // NOLINT
                pCubeNode->setWorldScale(glm::vec3(1.0F, 1.0F, 5.0F));     // NOLINT
            }
            {
                // Spawn cube 4.
                const auto pCubeNode = sgc::makeGc<MeshNode>();
                pCubeNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pCubeNode);
                pCubeNode->setWorldLocation(glm::vec3(3.0F, 4.5F, 0.75F)); // NOLINT
                pCubeNode->setWorldScale(glm::vec3(0.5F, 0.5F, 0.5F));     // NOLINT
            }
            {
                // Spawn cube 5.
                const auto pCubeNode = sgc::makeGc<MeshNode>();
                pCubeNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pCubeNode);
                pCubeNode->setWorldLocation(glm::vec3(3.0F, 5.5F, 10.0F)); // NOLINT
                pCubeNode->setWorldScale(glm::vec3(3.0F, 1.0F, 18.0F));    // NOLINT
            }
            {
                // Spawn cube 6.
                const auto pCubeNode = sgc::makeGc<MeshNode>();
                pCubeNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pCubeNode);
                pCubeNode->setWorldLocation(glm::vec3(9.0F, -8.0F, 1.0F)); // NOLINT
                pCubeNode->setWorldScale(glm::vec3(1.0F, 1.0F, 1.0F));     // NOLINT
            }
        });
    }

    void EditorGameInstance::onBeforeNewFrame(float timeSincePrevCallInSec) {
        // Get window and renderer.
        const auto pWindow = getWindow();
        const auto pRenderer = pWindow->getRenderer();
        const auto pRenderStats = pRenderer->getRenderStatistics();

        // Prepare variables to display frustum culling stats.
        const auto frameTime = 1000.0F / static_cast<float>(pRenderStats->getFramesPerSecond());
        const auto iFrustumCullingMeshesFrameTimePercent = static_cast<size_t>(
            pRenderStats->getTimeSpentLastFrameOnFrustumCullingMeshes() / frameTime * 100.0F); // NOLINT
        const auto iFrustumCullingLightsFrameTimePercent = static_cast<size_t>(
            pRenderStats->getTimeSpentLastFrameOnFrustumCullingLights() / frameTime * 100.0F); // NOLINT

        // Prepare frustum culling stats to display.
        const std::string sFrustumCullingStats = std::format(
            "frustum culled: meshes: {} (took {:.1F} ms), lights: {} (took {:.1F} ms) (~{}% "
            "of frame time)",
            pRenderStats->getLastFrameCulledMeshCount(),
            pRenderStats->getTimeSpentLastFrameOnFrustumCullingMeshes(),
            pRenderStats->getLastFrameCulledLightCount(),
            pRenderStats->getTimeSpentLastFrameOnFrustumCullingLights(),
            iFrustumCullingMeshesFrameTimePercent + iFrustumCullingLightsFrameTimePercent);

        // Show window title.
        pWindow->setTitle(std::format(
            "{} | {} {} | {} | FPS: {} | draw calls: {} | VRAM used: {} MB | {} | waiting GPU: {:.1F} ms",
            pEditorWindowTitle,
            pRenderer->getType() == RendererType::VULKAN ? "Vulkan" : "DirectX",
            pRenderer->getUsedApiVersion(),
            pRenderer->getCurrentlyUsedGpuName(),
            pRenderStats->getFramesPerSecond(),
            pRenderStats->getLastFrameDrawCallCount(),
            pRenderer->getResourceManager()->getUsedVideoMemoryInMb(),
            sFrustumCullingStats,
            pRenderStats->getTimeSpentLastFrameWaitingForGpu()));
    }

    void EditorGameInstance::spawnEditorNodesForNewWorld() {
        // Create camera.
        gcPointers.pCameraNode =
            EditorNodeCreationHelpers::createEditorNode<EditorCameraNode>("Editor's camera");

        // Setup camera.
        gcPointers.pCameraNode->setRelativeLocation(glm::vec3(-5.0F, 0.0F, 3.0F)); // NOLINT

        // Spawn camera.
        getWorldRootNode()->addChildNode(gcPointers.pCameraNode);

        // Make it active.
        getCameraManager()->setActiveCamera(gcPointers.pCameraNode);
    }
} // namespace ne
