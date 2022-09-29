#include "game/GameInstance.h"

// Custom.
#include "game/Window.h"
#include "render/IRenderer.h"
#include "window/GLFW.hpp"

namespace ne {
    GameInstance::GameInstance(Window* pGameWindow, InputManager* pInputManager) {
        this->pGameWindow = pGameWindow;
        this->pGame = pGameWindow->getRenderer()->getGame();
        this->pInputManager = pInputManager;
    }

    float GameInstance::getTotalApplicationTimeInSec() { return static_cast<float>(glfwGetTime()); }

    Window* GameInstance::getWindow() const { return pGameWindow; }

    InputManager* GameInstance::getInputManager() const { return pInputManager; }

    void GameInstance::addDeferredTask(const std::function<void()>& task) const {
        pGame->addDeferredTask(task);
    }

    void GameInstance::addTaskToThreadPool(const std::function<void()>& task) const {
        pGame->addTaskToThreadPool(task);
    }

    void GameInstance::createWorld(size_t iWorldSize) { pGame->createWorld(iWorldSize); }

    Node* GameInstance::getWorldRootNode() const { return pGame->getWorldRootNode(); }

    float GameInstance::getWorldTimeInSeconds() const { return pGame->getWorldTimeInSeconds(); }

} // namespace ne
