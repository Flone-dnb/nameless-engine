#include "game/IGameInstance.h"

// Custom.
#include "game/Window.h"
#include "render/IRenderer.h"
#include "window/GLFW.hpp"

namespace ne {
    IGameInstance::IGameInstance(Window* pGameWindow, InputManager* pInputManager) {
        this->pGameWindow = pGameWindow;
        this->pGame = pGameWindow->getRenderer()->getGame();
        this->pInputManager = pInputManager;
    }

    float IGameInstance::getTotalApplicationTimeInSec() { return static_cast<float>(glfwGetTime()); }

    Window* IGameInstance::getWindow() const { return pGameWindow; }

    InputManager* IGameInstance::getInputManager() const { return pInputManager; }

    void IGameInstance::addDeferredTask(const std::function<void()>& task) const {
        pGame->addDeferredTask(task);
    }

    void IGameInstance::addTaskToThreadPool(const std::function<void()>& task) const {
        pGame->addTaskToThreadPool(task);
    }

    void IGameInstance::createWorld(size_t iWorldSize) { pGame->createWorld(iWorldSize); }

} // namespace ne
