#include "game/GameInstance.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"

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

    void GameInstance::onInputActionEvent(
        const std::string& sActionName, KeyboardModifiers modifiers, bool bIsPressedDown) {
        std::scoped_lock guard(mtxBindedActionEvents.first);

        const auto it = mtxBindedActionEvents.second.find(sActionName);
        if (it == mtxBindedActionEvents.second.end()) {
            return;
        }

        it->second(modifiers, bIsPressedDown);
    }

    void
    GameInstance::onInputAxisEvent(const std::string& sAxisName, KeyboardModifiers modifiers, float input) {
        std::scoped_lock guard(mtxBindedAxisEvents.first);

        const auto it = mtxBindedAxisEvents.second.find(sAxisName);
        if (it == mtxBindedAxisEvents.second.end()) {
            return;
        }

        it->second(modifiers, input);
    }

    void GameInstance::addDeferredTask(const std::function<void()>& task) const {
        pGame->addDeferredTask(task);
    }

    void GameInstance::addTaskToThreadPool(const std::function<void()>& task) const {
        pGame->addTaskToThreadPool(task);
    }

    void GameInstance::createWorld(
        const std::function<void(const std::optional<Error>&)>& onCreated, size_t iWorldSize) {
        pGame->createWorld(onCreated, iWorldSize);
    }

    void GameInstance::loadNodeTreeAsWorld(
        const std::function<void(const std::optional<Error>&)>& onLoaded,
        const std::filesystem::path& pathToNodeTree,
        size_t iWorldSize) {
        pGame->loadNodeTreeAsWorld(onLoaded, pathToNodeTree, iWorldSize);
    }

    void GameInstance::queueGarbageCollection(const std::optional<std::function<void()>>& onFinished) {
        pGame->queueGarbageCollection(onFinished);
    }

    void GameInstance::setGarbageCollectorRunInterval(long long iGcRunIntervalInSec) {
        pGame->setGarbageCollectorRunInterval(iGcRunIntervalInSec);
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<std::string, std::function<void(KeyboardModifiers, bool)>>>*
    GameInstance::getActionEventBindings() {
        return &mtxBindedActionEvents;
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<std::string, std::function<void(KeyboardModifiers, float)>>>*
    GameInstance::getAxisEventBindings() {
        return &mtxBindedAxisEvents;
    }

    gc<Node> GameInstance::getWorldRootNode() const { return pGame->getWorldRootNode(); }

    float GameInstance::getWorldTimeInSeconds() const { return pGame->getWorldTimeInSeconds(); }

    size_t GameInstance::getWorldSize() const { return pGame->getWorldSize(); }

    size_t GameInstance::getCalledEveryFrameNodeCount() { return pGame->getCalledEveryFrameNodeCount(); }

} // namespace ne
