#include "game/GameInstance.h"

// Custom.
#include "game/GameManager.h"
#include "misc/Timer.h"
#include "io/Logger.h"
#include "game/Window.h"
#include "shader/ShaderManager.h"

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
            Logger::get().error("timers can no longer be created because the GameInstance will soon be "
                                "destroyed");
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
        unsigned int iActionId, KeyboardModifiers modifiers, bool bIsPressedDown) {
        std::scoped_lock guard(mtxBindedActionEvents.first);

        // Find this event in the registered events.
        const auto it = mtxBindedActionEvents.second.find(iActionId);
        if (it == mtxBindedActionEvents.second.end()) {
            return;
        }

        // Call user logic.
        it->second(modifiers, bIsPressedDown);
    }

    void GameInstance::onInputAxisEvent(unsigned int iAxisEventId, KeyboardModifiers modifiers, float input) {
        std::scoped_lock guard(mtxBindedAxisEvents.first);

        // Find this event in the registered events.
        const auto it = mtxBindedAxisEvents.second.find(iAxisEventId);
        if (it == mtxBindedAxisEvents.second.end()) {
            return;
        }

        // Call user logic.
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

    void GameInstance::executeShaderManagerSelfValidation() const {
        getWindow()->getRenderer()->getShaderManager()->performSelfValidation();
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, bool)>>>*
    GameInstance::getActionEventBindings() {
        return &mtxBindedActionEvents;
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, float)>>>*
    GameInstance::getAxisEventBindings() {
        return &mtxBindedAxisEvents;
    }

    sgc::GcPtr<Node> GameInstance::getWorldRootNode() const { return pGameManager->getWorldRootNode(); }

    float GameInstance::getWorldTimeInSeconds() const { return pGameManager->getWorldTimeInSeconds(); }

    size_t GameInstance::getWorldSize() const { return pGameManager->getWorldSize(); }

    size_t GameInstance::getTotalSpawnedNodeCount() { return pGameManager->getTotalSpawnedNodeCount(); }

    size_t GameInstance::getCalledEveryFrameNodeCount() {
        return pGameManager->getCalledEveryFrameNodeCount();
    }

} // namespace ne
