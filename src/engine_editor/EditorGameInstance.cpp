#include "EditorGameInstance.h"

// Custom.
#include "game/camera/TransientCamera.h"
#include "game/camera/CameraManager.h"
#include "game/nodes/MeshNode.h"
#include "game/Window.h"
#include "input/InputManager.h"
#include "misc/PrimitiveMeshGenerator.h"

namespace ne {
    EditorGameInstance::EditorGameInstance(
        Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
        : GameInstance(pWindow, pGameManager, pInputManager) {}

    void EditorGameInstance::onGameStarted() {
        // Create camera.
        pEditorCamera = std::make_shared<TransientCamera>();
        pEditorCamera->setCameraMovementSpeed(cameraMovementSpeed);
        getCameraManager()->setActiveCamera(pEditorCamera);
        pEditorCamera->setLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

        createWorld([this](std::optional<Error> optionalWorldError) {
            // Spawn sample mesh.
            const auto pMeshNode = gc_new<MeshNode>();
            const auto mtxMeshData = pMeshNode->getMeshData();
            {
                std::scoped_lock guard(*mtxMeshData.first);
                (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
            }

            getWorldRootNode()->addChildNode(pMeshNode);
            pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

            bindCameraInput();
        });
    }

    void EditorGameInstance::onMouseMove(int iXOffset, int iYOffset) {
        if (!bIsMouseCursorCaptured) {
            return;
        }

        auto currentRotation = pEditorCamera->getFreeCameraRotation();
        currentRotation.z += iXOffset * cameraRotationSensitivity;
        currentRotation.y -= iYOffset * cameraRotationSensitivity;
        pEditorCamera->setFreeCameraRotation(currentRotation);
    }

    void EditorGameInstance::bindCameraInput() {
        const auto pInputManager = getInputManager();

        // Register events.
        auto optionalError = pInputManager->addAxisEvent(
            InputEventNames::moveForwardAxis, {std::make_pair(KeyboardKey::KEY_W, KeyboardKey::KEY_S)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addEntry();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        optionalError = pInputManager->addAxisEvent(
            InputEventNames::moveRightAxis, {std::make_pair(KeyboardKey::KEY_D, KeyboardKey::KEY_A)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addEntry();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        optionalError = pInputManager->addAxisEvent(
            InputEventNames::moveUpAxis, {std::make_pair(KeyboardKey::KEY_E, KeyboardKey::KEY_Q)});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addEntry();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        optionalError =
            pInputManager->addActionEvent(InputEventNames::captureMouseCursorAction, {MouseButton::RIGHT});
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addEntry();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Bind callbacks.
        {
            const auto pMtxAxisEventBindings = getAxisEventBindings();
            std::scoped_lock guard(pMtxAxisEventBindings->first);

            pMtxAxisEventBindings->second[InputEventNames::moveForwardAxis] =
                [this](KeyboardModifiers modifiers, float input) {
                    if (!bIsMouseCursorCaptured) {
                        return;
                    }
                    pEditorCamera->setFreeCameraForwardMovement(input);
                };

            pMtxAxisEventBindings->second[InputEventNames::moveRightAxis] =
                [this](KeyboardModifiers modifiers, float input) {
                    if (!bIsMouseCursorCaptured) {
                        return;
                    }
                    pEditorCamera->setFreeCameraRightMovement(input);
                };

            pMtxAxisEventBindings->second[InputEventNames::moveUpAxis] =
                [this](KeyboardModifiers modifiers, float input) {
                    if (!bIsMouseCursorCaptured) {
                        return;
                    }
                    pEditorCamera->setFreeCameraWorldUpMovement(input);
                };
        }
        {
            const auto pMtxActionEventBindings = getActionEventBindings();
            std::scoped_lock guard(pMtxActionEventBindings->first);

            pMtxActionEventBindings->second[InputEventNames::captureMouseCursorAction] =
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
        }
    }
} // namespace ne
