#include "EditorGameInstance.h"

// Standard.
#include <format>

// Custom.
#include "game/camera/TransientCamera.h"
#include "game/camera/CameraManager.h"
#include "game/nodes/MeshNode.h"
#include "game/Window.h"
#include "input/InputManager.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "game/nodes/EnvironmentNode.h"
#include "game/nodes/light/DirectionalLightNode.h"
#include "game/nodes/light/PointLightNode.h"

namespace ne {
    const char* EditorGameInstance::getEditorWindowTitle() { return pEditorWindowTitle; }

    EditorGameInstance::EditorGameInstance(
        Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
        : GameInstance(pWindow, pGameManager, pInputManager) {}

    std::shared_ptr<TransientCamera> EditorGameInstance::getEditorCamera() { return pEditorCamera; }

    void EditorGameInstance::onGameStarted() {
        // Create and setup camera.
        pEditorCamera = std::make_shared<TransientCamera>();
        pEditorCamera->setLocation(glm::vec3(-2.0F, -1.0F, 1.0F));
        updateCameraSpeed();

        // Make it active.
        getCameraManager()->setActiveCamera(pEditorCamera);

        // Bind camera to input.
        bindCameraInput();

        // Create world.
        createWorld([this](const std::optional<Error>& optionalWorldError) {
            if (optionalWorldError.has_value()) [[unlikely]] {
                auto error = optionalWorldError.value();
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Spawn environment node.
            const auto pEnvironmentNode = gc_new<EnvironmentNode>();
            pEnvironmentNode->setAmbientLight(glm::vec3(0.1F, 0.1F, 0.1F)); // NOLINT
            getWorldRootNode()->addChildNode(pEnvironmentNode);

            // Spawn directional light.
            const auto pDirectionalLightNode = gc_new<DirectionalLightNode>();
            getWorldRootNode()->addChildNode(pDirectionalLightNode);
            pDirectionalLightNode->setWorldRotation(MathHelpers::convertDirectionToRollPitchYaw(
                glm::normalize(glm::vec3(0.5F, -1.0F, -1.0F)))); // NOLINT
            pDirectionalLightNode->setLightIntensity(0.7F);      // NOLINT

            // Spawn point light.
            const auto pPointLightNode = gc_new<PointLightNode>();
            getWorldRootNode()->addChildNode(pPointLightNode);
            pPointLightNode->setLightColor(glm::vec3(1.0F, 0.0F, 0.0F));
            pPointLightNode->setWorldLocation(glm::vec3(3.0F, 4.0F, 4.0F));

            // Spawn floor.
            const auto pFloorNode = gc_new<MeshNode>();
            pFloorNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
            getWorldRootNode()->addChildNode(pFloorNode);
            pFloorNode->setWorldScale(glm::vec3(100.0F, 100.0F, 1.0F)); // NOLINT
            pFloorNode->getMaterial()->setRoughness(0.9F);

            // Spawn cube.
            const auto pCubeNode = gc_new<MeshNode>();
            pCubeNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
            getWorldRootNode()->addChildNode(pCubeNode);
            pCubeNode->setWorldLocation(glm::vec3(0.0F, 0.0F, 1.0F)); // NOLINT
        });
    }

    void EditorGameInstance::onMouseMove(double xOffset, double yOffset) {
        if (!bIsMouseCursorCaptured) {
            return;
        }

        auto currentRotation = pEditorCamera->getFreeCameraRotation();
        currentRotation.z += static_cast<float>(xOffset * cameraRotationSensitivity);
        currentRotation.y -= static_cast<float>(yOffset * cameraRotationSensitivity);
        pEditorCamera->setFreeCameraRotation(currentRotation);
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
            "frustum culled: meshes: {} (took {:.1F} ms), lights: {} (took {:.1F} ms), (~{}% "
            "of frame time)",
            pRenderStats->getLastFrameCulledMeshCount(),
            pRenderStats->getTimeSpentLastFrameOnFrustumCullingMeshes(),
            pRenderStats->getLastFrameCulledLightCount(),
            pRenderStats->getTimeSpentLastFrameOnFrustumCullingLights(),
            iFrustumCullingMeshesFrameTimePercent + iFrustumCullingLightsFrameTimePercent);

        // Show window title.
        pWindow->setTitle(std::format(
            "{} | FPS: {} | draw calls: {} | VRAM used: {} MB | {} | waiting GPU: {:.1F} ms",
            pEditorWindowTitle,
            pRenderStats->getFramesPerSecond(),
            pRenderStats->getLastFrameDrawCallCount(),
            pRenderer->getResourceManager()->getUsedVideoMemoryInMb(),
            sFrustumCullingStats,
            pRenderStats->getTimeSpentLastFrameWaitingForGpu()));
    }

    void EditorGameInstance::bindCameraInput() {
        const auto pInputManager = getInputManager();

        // Register events.

        // Move forward.
        auto optionalError = pInputManager->addAxisEvent(
            InputEventIds::Axis::iMoveForward, {std::make_pair(KeyboardKey::KEY_W, KeyboardKey::KEY_S)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Move right.
        optionalError = pInputManager->addAxisEvent(
            InputEventIds::Axis::iMoveRight, {std::make_pair(KeyboardKey::KEY_D, KeyboardKey::KEY_A)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Move up.
        optionalError = pInputManager->addAxisEvent(
            InputEventIds::Axis::iMoveUp, {std::make_pair(KeyboardKey::KEY_E, KeyboardKey::KEY_Q)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Capture mouse.
        optionalError =
            pInputManager->addActionEvent(InputEventIds::Action::iCaptureMouseCursor, {MouseButton::RIGHT});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Increase camera speed.
        optionalError = pInputManager->addActionEvent(
            InputEventIds::Action::iIncreaseCameraSpeed, {KeyboardKey::KEY_LEFT_SHIFT});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Decrease camera speed.
        optionalError = pInputManager->addActionEvent(
            InputEventIds::Action::iDecreaseCameraSpeed, {KeyboardKey::KEY_LEFT_CONTROL});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Bind callbacks.

        // Axis events.
        {
            const auto pMtxAxisEventBindings = getAxisEventBindings();
            std::scoped_lock guard(pMtxAxisEventBindings->first);

            pMtxAxisEventBindings->second[InputEventIds::Axis::iMoveForward] =
                [this](KeyboardModifiers modifiers, float input) {
                    if (!bIsMouseCursorCaptured) {
                        return;
                    }
                    pEditorCamera->setFreeCameraForwardMovement(input);
                };

            pMtxAxisEventBindings->second[InputEventIds::Axis::iMoveRight] =
                [this](KeyboardModifiers modifiers, float input) {
                    if (!bIsMouseCursorCaptured) {
                        return;
                    }
                    pEditorCamera->setFreeCameraRightMovement(input);
                };

            pMtxAxisEventBindings->second[InputEventIds::Axis::iMoveUp] =
                [this](KeyboardModifiers modifiers, float input) {
                    if (!bIsMouseCursorCaptured) {
                        return;
                    }
                    pEditorCamera->setFreeCameraWorldUpMovement(input);
                };
        }

        // Action events.
        {
            const auto pMtxActionEventBindings = getActionEventBindings();
            std::scoped_lock guard(pMtxActionEventBindings->first);

            pMtxActionEventBindings->second[InputEventIds::Action::iCaptureMouseCursor] =
                [this](KeyboardModifiers modifiers, bool bIsPressed) {
                    bIsMouseCursorCaptured = bIsPressed;
                    getWindow()->setCursorVisibility(!bIsMouseCursorCaptured);
                    if (!bIsMouseCursorCaptured) {
                        // Reset any existing input.
                        pEditorCamera->setFreeCameraForwardMovement(0.0F);
                        pEditorCamera->setFreeCameraRightMovement(0.0F);
                        pEditorCamera->setFreeCameraWorldUpMovement(0.0F);
                    }
                };

            pMtxActionEventBindings->second[InputEventIds::Action::iIncreaseCameraSpeed] =
                [this](KeyboardModifiers modifiers, bool bIsPressed) {
                    bShouldIncreaseCameraSpeed = bIsPressed;
                    updateCameraSpeed();
                };

            pMtxActionEventBindings->second[InputEventIds::Action::iDecreaseCameraSpeed] =
                [this](KeyboardModifiers modifiers, bool bIsPressed) {
                    bShouldDecreaseCameraSpeed = bIsPressed;
                    updateCameraSpeed();
                };
        }
    }

    void EditorGameInstance::updateCameraSpeed() {
        auto currentSpeed = cameraMovementSpeed;

        if (bShouldIncreaseCameraSpeed) {
            currentSpeed *= cameraSpeedIncreaseMultiplier;
        } else if (bShouldDecreaseCameraSpeed) {
            currentSpeed *= cameraSpeedDecreaseMultiplier;
        }

        pEditorCamera->setCameraMovementSpeed(currentSpeed);
    }
} // namespace ne
