#include "game/GameInstance.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"
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

    long long GameInstance::getGarbageCollectorRunIntervalInSec() {
        return pGame->getGarbageCollectorRunIntervalInSec();
    }

    void GameInstance::addDeferredTask(const std::function<void()>& task) const {
        pGame->addDeferredTask(task);
    }

    void GameInstance::addTaskToThreadPool(const std::function<void()>& task) const {
        pGame->addTaskToThreadPool(task);
    }

    void GameInstance::createWorld(size_t iWorldSize) { pGame->createWorld(iWorldSize); }

    std::optional<Error>
    GameInstance::loadNodeTreeAsWorld(const std::filesystem::path& pathToNodeTree, size_t iWorldSize) {
        auto optionalError = pGame->loadNodeTreeAsWorld(pathToNodeTree, iWorldSize);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        return {};
    }

    void GameInstance::queueGarbageCollection(std::optional<std::function<void()>> onFinished) {
        pGame->queueGarbageCollection(onFinished);
    }

    void GameInstance::setGarbageCollectorRunInterval(long long iGcRunIntervalInSec) {
        pGame->setGarbageCollectorRunInterval(iGcRunIntervalInSec);
    }

    gc<Node> GameInstance::getWorldRootNode() const { return pGame->getWorldRootNode(); }

    float GameInstance::getWorldTimeInSeconds() const { return pGame->getWorldTimeInSeconds(); }

    size_t GameInstance::getCalledEveryFrameNodeCount() { return pGame->getCalledEveryFrameNodeCount(); }

} // namespace ne
