#include "IGameInstance.h"

// Custom.
#include "window/GLFW.hpp"

namespace ne {
    IGameInstance::IGameInstance(Window *pGameWindow) { this->pGameWindow = pGameWindow; }

    float IGameInstance::getTotalApplicationTimeInSec() { return static_cast<float>(glfwGetTime()); }

    Window *IGameInstance::getGameWindow() const { return pGameWindow; }
} // namespace ne
