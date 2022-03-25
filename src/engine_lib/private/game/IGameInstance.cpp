#include "game/IGameInstance.h"

// Custom.
#include "window/GLFW.hpp"

namespace ne {
    IGameInstance::IGameInstance(Window *pGameWindow, InputManager *pInputManager) {
        this->pGameWindow = pGameWindow;
        this->pInputManager = pInputManager;
    }

    float IGameInstance::getTotalApplicationTimeInSec() { return static_cast<float>(glfwGetTime()); }

    Window *IGameInstance::getGameWindow() const { return pGameWindow; }

    InputManager *IGameInstance::getInputManager() const { return pInputManager; }
} // namespace ne
