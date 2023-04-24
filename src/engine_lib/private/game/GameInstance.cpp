#include "game/GameInstance.h"

// Custom.
#include "game/GameManager.h"
#include "misc/Timer.h"

namespace ne {
    GameInstance::GameInstance(Window* pGameWindow, GameManager* pGameManager, InputManager* pInputManager) {
        this->pGameWindow = pGameWindow;
        this->pGameManager = pGameManager;
        this->pInputManager = pInputManager;
    }

    float GameInstance::getTotalApplicationTimeInSec() { return static_cast<float>(glfwGetTime()); }

    Window* GameInstance::getWindow() const { return pGameWindow; }

    CameraManager* GameInstance::getCameraManager() const { return pGameManager->getCameraManager(); }

    InputManager* GameInstance::getInputManager() const { return pInputManager; }

    long long GameInstance::getGarbageCollectorRunIntervalInSec() {
        return pGameManager->getGarbageCollectorRunIntervalInSec();
    }

    Timer* GameInstance::createTimer(const std::string& sTimerName) {
        std::scoped_lock guard(mtxCreatedTimers.first);

        if (!bAllowCreatingTimers) {
            Logger::get().error(
                "timers can no longer be created because the GameInstance will soon be "
                "destroyed",
                sGameInstanceLogCategory);
            return nullptr;
        }

        // Create timer.
        auto pNewTimer = std::unique_ptr<Timer>(new Timer(sTimerName));

        // Set validator.
        pNewTimer->setCallbackValidator([this, pTimer = pNewTimer.get()](size_t iStartCount) -> bool {
            if (!bAllowCreatingTimers) {
                // Being destroyed, we don't expect any timers to be called at this point.
                return false;
            }

            if (iStartCount != pTimer->getStartCount()) {
                // The timer was stopped and started (probably with some other callback).
                return false;
            }

            return !pTimer->isStopped();
        });

        // Add to our array of created timers.
        const auto pRawTimer = pNewTimer.get();
        mtxCreatedTimers.second.push_back(std::move(pNewTimer));

        return pRawTimer;
    }

    void GameInstance::stopAndDisableCreatedTimers() {
        std::scoped_lock guard(mtxCreatedTimers.first);

        for (const auto& pTimer : mtxCreatedTimers.second) {
            pTimer->stop(true);
        }

        bAllowCreatingTimers = false;
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
        pGameManager->addDeferredTask(task);
    }

    void GameInstance::addTaskToThreadPool(const std::function<void()>& task) const {
        pGameManager->addTaskToThreadPool(task);
    }

    void GameInstance::createWorld(
        const std::function<void(const std::optional<Error>&)>& onCreated, size_t iWorldSize) {
        pGameManager->createWorld(onCreated, iWorldSize);
    }

    void GameInstance::loadNodeTreeAsWorld(
        const std::function<void(const std::optional<Error>&)>& onLoaded,
        const std::filesystem::path& pathToNodeTree,
        size_t iWorldSize) {
        pGameManager->loadNodeTreeAsWorld(onLoaded, pathToNodeTree, iWorldSize);
    }

    void GameInstance::queueGarbageCollection(
        bool bForce, const std::optional<std::function<void()>>& onFinished) {
        pGameManager->queueGarbageCollection(bForce, onFinished);
    }

    void GameInstance::setGarbageCollectorRunInterval(long long iGcRunIntervalInSec) {
        pGameManager->setGarbageCollectorRunInterval(iGcRunIntervalInSec);
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

    gc<Node> GameInstance::getWorldRootNode() const { return pGameManager->getWorldRootNode(); }

    float GameInstance::getWorldTimeInSeconds() const { return pGameManager->getWorldTimeInSeconds(); }

    size_t GameInstance::getWorldSize() const { return pGameManager->getWorldSize(); }

    size_t GameInstance::getTotalSpawnedNodeCount() { return pGameManager->getTotalSpawnedNodeCount(); }

    size_t GameInstance::getCalledEveryFrameNodeCount() {
        return pGameManager->getCalledEveryFrameNodeCount();
    }

} // namespace ne
