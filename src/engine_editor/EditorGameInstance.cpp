#include "EditorGameInstance.h"

namespace ne {
    EditorGameInstance::EditorGameInstance(
        Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
        : GameInstance(pWindow, pGameManager, pInputManager) {}
} // namespace ne
