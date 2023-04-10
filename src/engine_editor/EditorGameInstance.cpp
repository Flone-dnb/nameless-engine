#include "EditorGameInstance.h"

// Custom.
#include "game/camera/TransientCamera.h"
#include "game/camera/CameraManager.h"

namespace ne {
    EditorGameInstance::EditorGameInstance(
        Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
        : GameInstance(pWindow, pGameManager, pInputManager) {}

    void EditorGameInstance::onGameStarted() {
        getCameraManager()->setActiveCamera(std::make_unique<TransientCamera>());
    }
} // namespace ne
