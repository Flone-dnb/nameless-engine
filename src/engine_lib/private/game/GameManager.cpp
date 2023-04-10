#include "GameManager.h"

// Standard.
#include <thread>
#include <sstream>

// Custom.
#include "io/Logger.h"
#include "io/Serializable.h"
#include "misc/ProjectPaths.h"
#include "misc/GC.hpp"
#include "render/Renderer.h"
#include "materials/Shader.h"
#include "render/general/pso/PsoManager.h"
#include "io/FieldSerializerManager.h"
#include "game/camera/CameraManager.h"

// External.
#include "fmt/core.h"

namespace ne {
    // Static pointer for accessing last created game.
    static GameManager* pLastCreatedGameManager = nullptr;

    GameManager::GameManager(Window* pWindow) {
        this->pWindow = pWindow;

        // Update static pointer.
        Logger::get().info(
            "new GameManager is created, updating static GameManager pointer", sGameLogCategory);
        pLastCreatedGameManager = this;

        // Make sure that `res` directory is set and exists.
        // (intentionally ignore result, will show an error if not exists)
        ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT);

        // Save ID of this thread (should be main thread).
        mainThreadId = std::this_thread::get_id();

        // Register engine serializers.
        FieldSerializerManager::registerEngineFieldSerializers();

        // Run GC for the first time to setup things (I guess, first scan is usually not that fast).
        gc_collector()->collect();
        lastGcRunTime = std::chrono::steady_clock::now();
        Logger::get().info(
            fmt::format("garbage collector will run every {} seconds", iGcRunIntervalInSec),
            sGameLogCategory);

#if defined(DEBUG)
        SerializableObjectFieldSerializer::checkGuidUniqueness();
#endif

        // Create renderer.
        pRenderer = Renderer::create(this);

        // Create camera manager.
        pCameraManager = std::make_unique<CameraManager>();
    }

    void GameManager::destroy() {
        if (bIsBeingDestroyed) {
            return;
        }
        bIsBeingDestroyed = true;

        // Wait for GPU to finish all work.
        // Make sure no GPU resource is used.
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Destroy world if needed.
        {
            std::scoped_lock guard(mtxWorld.first);

            if (mtxWorld.second != nullptr) {
                // Destroy world before game instance, so that no node
                // will reference game instance.
                mtxWorld.second->destroyWorld();

                // Process all `node despawned` tasks that will notify world.
                std::scoped_lock guard(mtxDeferredTasks.first);
                executeDeferredTasks();

                // Can safely destroy world object now.
                mtxWorld.second = nullptr;
            }
        }

        // Make sure thread pool and deferred tasks are finished.
        threadPool.stop();
        {
            std::scoped_lock guard(mtxDeferredTasks.first);
            executeDeferredTasks(); // finish all deferred tasks
            bShouldAcceptNewDeferredTasks = false;
        }

        // Explicitly destroy game instance before running GC so that if game instance holds any GC
        // pointers they will be cleared.
        pGameInstance = nullptr;

        // Run GC for the last time.
        Logger::get().info(
            "Game object is being destroyed, running garbage collector...", sGarbageCollectorLogCategory);
        gc_collector()->fullCollect();

        // Log results.
        Logger::get().info(
            fmt::format(
                "garbage collector has finished: "
                "freed {} object(s) ({} left alive)",
                gc_collector()->getLastFreedObjectsCount(),
                gc_collector()->getAliveObjectsCount()),
            sGarbageCollectorLogCategory);

        // After GC has finished and all nodes were deleted.
        // Clear global game pointer.
        Logger::get().info("clearing static GameManager pointer", sGameLogCategory);
        pLastCreatedGameManager = nullptr;

        // See if there are any nodes alive.
        const auto iNodesAlive = Node::getAliveNodeCount();
        if (iNodesAlive != 0) {
            Logger::get().error(
                fmt::format(
                    "the game was destroyed and a full garbage collection was run but there are still "
                    "{} node(s) alive, here are a few reasons why this may happen:\n{}",
                    iNodesAlive,
                    sGcLeakReasons),
                sGameLogCategory);
        }

        // See if there are any gc objects left.
        const auto iGcObjectsLeft = gc_collector()->getAliveObjectsCount();
        if (iGcObjectsLeft != 0) {
            Logger::get().error(
                fmt::format(
                    "the game was destroyed and a full garbage collection was run but there are still "
                    "{} gc object(s) alive, here are a few reasons why this may happen:\n{}",
                    iGcObjectsLeft,
                    sGcLeakReasons),
                sGameLogCategory);
        }

        // Explicitly destroy the renderer to check how much shaders left in the memory.
        pRenderer = nullptr;
        const auto iTotalShadersInMemory = Shader::getTotalAmountOfLoadedShaders();
        if (iTotalShadersInMemory != 0) {
            Logger::get().error(
                fmt::format(
                    "the renderer was destroyed but there are still {} shader(s) left in the memory",
                    iTotalShadersInMemory),
                sGameLogCategory);
        }
    }

    void GameManager::onTickFinished() { runGarbageCollection(); }

    void GameManager::runGarbageCollection(bool bForce) {
        // Make sure this function is being executed on the main thread.
        const auto currentThreadId = std::this_thread::get_id();
        if (currentThreadId != mainThreadId) [[unlikely]] {
            std::stringstream currentThreadIdString;
            currentThreadIdString << currentThreadId;

            std::stringstream mainThreadIdString;
            mainThreadIdString << mainThreadId;

            Error err(fmt::format(
                "an attempt was made to call a function that should only be called on the main thread "
                "in a non main thread (main thread ID: {}, current thread ID: {})",
                mainThreadIdString.str(),
                currentThreadIdString.str()));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        if (!bForce) {
            // Check if enough time has passed since the last garbage collection.
            const auto iTimeSinceLastGcInSec = std::chrono::duration_cast<std::chrono::seconds>(
                                                   std::chrono::steady_clock::now() - lastGcRunTime)
                                                   .count();
            if (iTimeSinceLastGcInSec < iGcRunIntervalInSec) {
                return;
            }
        }

        // Finish all deferred tasks and lock them until not finished with garbage collector.
        // We want to finish all deferred tasks right now because there might be
        // node member functions waiting to be executed - execute them and only
        // then delete nodes.
        std::scoped_lock deferredTasksGuard(mtxDeferredTasks.first); // don't allow new tasks before finished
        executeDeferredTasks();

        // Log start.
        Logger::get().info("running garbage collector...", sGarbageCollectorLogCategory);

        // Save time to measure later.
        const auto start = std::chrono::steady_clock::now();

        // Run garbage collector.
        gc_collector()->collect();

        // Measure the time it took to run garbage collector.
        const auto end = std::chrono::steady_clock::now();
        const auto durationInMs =
            static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) *
            0.000001F;

        // Convert time to string. Limit precision to 1 digit.
        std::stringstream durationStream;
        durationStream << std::fixed << std::setprecision(1) << durationInMs;

        // Log results.
        Logger::get().info(
            fmt::format(
                "garbage collector has finished, took {} millisecond(s): "
                "freed {} object(s) ({} left alive)",
                durationStream.str(),
                gc_collector()->getLastFreedObjectsCount(),
                gc_collector()->getAliveObjectsCount()),
            sGarbageCollectorLogCategory);

        // Save current time.
        lastGcRunTime = std::chrono::steady_clock::now();
    }

    GameManager::~GameManager() { destroy(); }

    GameManager* GameManager::get() { return pLastCreatedGameManager; }

    void GameManager::setGarbageCollectorRunInterval(long long iGcRunIntervalInSec) {
        this->iGcRunIntervalInSec = std::clamp<long long>(iGcRunIntervalInSec, 30, 300); // NOLINT
    }

    void
    GameManager::queueGarbageCollection(bool bForce, const std::optional<std::function<void()>>& onFinished) {
        addDeferredTask([this, bForce, onFinished]() {
            runGarbageCollection(bForce);
            if (onFinished.has_value()) {
                onFinished->operator()();
            }
        });
    }

    void GameManager::onBeforeNewFrame(float timeSincePrevCallInSec) {
        // Save delta time.
        timeSincePrevFrameInSec = timeSincePrevCallInSec;

        pRenderer->getShaderManager()->performSelfValidation();

        // Call on game instance.
        pGameInstance->onBeforeNewFrame(timeSincePrevCallInSec);

        {
            // Call on all tickable nodes.
            std::scoped_lock guard(mtxWorld.first);
            if (mtxWorld.second == nullptr) {
                return;
            }

            const auto pCalledEveryFrameNodes = mtxWorld.second->getCalledEveryFrameNodes();

            auto callTick = [&](std::pair<std::recursive_mutex, std::vector<Node*>>* pTickGroup) {
                std::scoped_lock nodesGuard(pTickGroup->first);

                std::vector<Node*>* pNodes = &pTickGroup->second;
                for (auto it = pNodes->begin(); it != pNodes->end(); ++it) {
                    (*it)->onBeforeNewFrame(timeSincePrevCallInSec);
                }
            };

            callTick(&pCalledEveryFrameNodes->mtxFirstTickGroup);
            callTick(&pCalledEveryFrameNodes->mtxSecondTickGroup);
        }

        // Call on camera manager.
        pCameraManager->onBeforeNewFrame(timeSincePrevCallInSec);
    }

    void GameManager::executeDeferredTasks() {
        std::scoped_lock guard(mtxDeferredTasks.first);

        if (!bShouldAcceptNewDeferredTasks) // check under mutex
        {
            return;
        }

        while (!mtxDeferredTasks.second.empty()) {
            auto task = std::move(mtxDeferredTasks.second.front());
            mtxDeferredTasks.second.pop(); // remove first and only then execute to avoid recursion
            task();
        }
    }

    void GameManager::addTaskToThreadPool(const std::function<void()>& task) {
        if (bIsBeingDestroyed) [[unlikely]] {
            // Destructor is running, don't queue any more tasks.
            return;
        }

        threadPool.addTask(task);
    }

    void GameManager::createWorld(
        const std::function<void(const std::optional<Error>&)>& onCreated, size_t iWorldSize) {
        addDeferredTask([=]() {
            std::scoped_lock guard(mtxWorld.first);

            destroyAndCleanExistingWorld();

            mtxWorld.second = World::createWorld(this, iWorldSize);

            onCreated({});
        });
    }

    void GameManager::loadNodeTreeAsWorld(
        const std::function<void(const std::optional<Error>&)>& onLoaded,
        const std::filesystem::path& pathToNodeTree,
        size_t iWorldSize) {
        addDeferredTask([=]() {
            std::scoped_lock guard(mtxWorld.first);

            destroyAndCleanExistingWorld();

            // Load new world.
            auto result = World::loadNodeTreeAsWorld(this, pathToNodeTree, iWorldSize);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(result);
                error.addEntry();
                onLoaded(error);
                return;
            }

            mtxWorld.second = std::get<std::unique_ptr<World>>(std::move(result));

            onLoaded({});
        });
    }

    gc<Node> GameManager::getWorldRootNode() {
        std::scoped_lock guard(mtxWorld.first);

        if (mtxWorld.second == nullptr) {
            return nullptr;
        }

        return mtxWorld.second->getRootNode();
    }

    float GameManager::getWorldTimeInSeconds() {
        std::scoped_lock guard(mtxWorld.first);

        if (mtxWorld.second == nullptr) {
            return 0.0F;
        }

        return mtxWorld.second->getWorldTimeInSeconds();
    }

    size_t GameManager::getWorldSize() {
        std::scoped_lock guard(mtxWorld.first);

        if (mtxWorld.second == nullptr) {
            return 0;
        }

        return mtxWorld.second->getWorldSize();
    }

    size_t GameManager::getTotalSpawnedNodeCount() {
        std::scoped_lock guard(mtxWorld.first);

        if (mtxWorld.second == nullptr) {
            return 0;
        }

        return mtxWorld.second->getTotalSpawnedNodeCount();
    }

    size_t GameManager::getCalledEveryFrameNodeCount() {
        std::scoped_lock guard(mtxWorld.first);

        if (mtxWorld.second == nullptr) {
            return 0;
        }

        return mtxWorld.second->getCalledEveryFrameNodeCount();
    }

    void GameManager::onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onKeyboardInput(key, modifiers, bIsPressedDown);

        triggerActionEvents(key, modifiers, bIsPressedDown);

        triggerAxisEvents(key, modifiers, bIsPressedDown);
    }

    void GameManager::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onMouseInput(button, modifiers, bIsPressedDown);

        triggerActionEvents(button, modifiers, bIsPressedDown);
    }

    void GameManager::onMouseMove(int iXOffset, int iYOffset) {
        pGameInstance->onMouseMove(iXOffset, iYOffset);

        // Call on nodes that receive input.
        std::scoped_lock guard(mtxWorld.first);
        if (mtxWorld.second) {
            const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

            std::scoped_lock nodesGuard(pReceivingInputNodes->first);
            std::vector<Node*>* pNodes = &pReceivingInputNodes->second;
            for (auto it = pNodes->begin(); it != pNodes->end(); ++it) {
                (*it)->onMouseMove(iXOffset, iYOffset);
            }
        }
    }

    void GameManager::onMouseScrollMove(int iOffset) {
        pGameInstance->onMouseScrollMove(iOffset);

        // Call on nodes that receive input.
        std::scoped_lock guard(mtxWorld.first);
        if (mtxWorld.second) {
            const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

            std::scoped_lock nodesGuard(pReceivingInputNodes->first);
            std::vector<Node*>* pNodes = &pReceivingInputNodes->second;
            for (auto it = pNodes->begin(); it != pNodes->end(); ++it) {
                (*it)->onMouseScrollMove(iOffset);
            }
        }
    }

    void GameManager::onWindowFocusChanged(bool bIsFocused) const {
        pGameInstance->onWindowFocusChanged(bIsFocused);
    }

    void GameManager::onWindowClose() const { pGameInstance->onWindowClose(); }

    void GameManager::addDeferredTask(const std::function<void()>& task) {
        if (!bShouldAcceptNewDeferredTasks) {
            // Destructor is running, don't queue any more tasks.
            return;
        }

        {
            std::scoped_lock guard(mtxDeferredTasks.first);
            mtxDeferredTasks.second.push(task);
        }

        if (pGameInstance == nullptr) {
            // Tick is not started yet but we already have some tasks (probably engine internal calls).
            // Execute them now.
            executeDeferredTasks();
        }
    }

    Window* GameManager::getWindow() const { return pWindow; }

    GameInstance* GameManager::getGameInstance() const { return pGameInstance.get(); }

    CameraManager* GameManager::getCameraManager() const { return pCameraManager.get(); }

    float GameManager::getTimeSincePrevFrameInSec() const { return timeSincePrevFrameInSec; }

    long long GameManager::getGarbageCollectorRunIntervalInSec() const { return iGcRunIntervalInSec; }

    bool GameManager::isBeingDestroyed() const { return bIsBeingDestroyed; }

    void GameManager::triggerActionEvents( // NOLINT
        std::variant<KeyboardKey, MouseButton> key,
        KeyboardModifiers modifiers,
        bool bIsPressedDown) {
        std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxActionEvents);
        if (inputManager.actionEvents.empty()) {
            return;
        }

        const auto it = inputManager.actionEvents.find(key);
        if (it == inputManager.actionEvents.end()) {
            return;
        }

        // TODO: find a better approach:
        // copying set of actions because it can be modified in onInputActionEvent by user code
        // while we are iterating over it (the user should be able to modify it).
        // This should not be that bad due to the fact that a key will usually have
        // only one action associated with it.
        // + update InputManager documentation.
        const auto actionsCopy = it->second;
        for (const auto& sActionName : actionsCopy) {
            // Update state.
            const auto stateIt = inputManager.actionState.find(sActionName);
            if (stateIt == inputManager.actionState.end()) {
                Logger::get().error(
                    fmt::format(
                        "input manager returned 0 "
                        "states for '{}' action event",
                        sActionName),
                    sGameLogCategory);
            } else {
                std::pair<std::vector<ActionState>, bool /* action state */>& statePair = stateIt->second;

                // Mark state.
                bool bSet = false;
                for (auto& actionKey : statePair.first) {
                    if (actionKey.key == key) {
                        actionKey.bIsPressed = bIsPressedDown;
                        bSet = true;
                        break;
                    }
                }

                if (!bSet) {
                    if (std::holds_alternative<KeyboardKey>(key)) {
                        Logger::get().error(
                            fmt::format(
                                "could not find key '{}' in key "
                                "states for '{}' action event",
                                getKeyName(std::get<KeyboardKey>(key)),
                                sActionName),
                            sGameLogCategory);
                    } else {
                        Logger::get().error(
                            fmt::format(
                                "could not find mouse button '{}' in key "
                                "states for '{}' action event",
                                static_cast<int>(std::get<MouseButton>(key)),
                                sActionName),
                            sGameLogCategory);
                    }
                }

                bool bNewState = bIsPressedDown;

                if (!bIsPressedDown) {
                    // See if other button are pressed.
                    for (const auto actionKey : statePair.first) {
                        if (actionKey.bIsPressed) {
                            bNewState = true;
                            break;
                        }
                    }
                }

                if (bNewState != statePair.second) {
                    statePair.second = bNewState;
                    pGameInstance->onInputActionEvent(sActionName, modifiers, bNewState);

                    // Call on nodes that receive input.
                    std::scoped_lock guard(mtxWorld.first);
                    if (mtxWorld.second != nullptr) {
                        const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

                        std::scoped_lock nodesGuard(pReceivingInputNodes->first);
                        for (const auto& pNode : pReceivingInputNodes->second) {
                            pNode->onInputActionEvent(sActionName, modifiers, bNewState);
                        }
                    }
                }
            }
        }
    }

    void GameManager::triggerAxisEvents( // NOLINT: too complex
        KeyboardKey key,
        KeyboardModifiers modifiers,
        bool bIsPressedDown) {
        std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxAxisEvents);
        if (inputManager.axisEvents.empty()) {
            return;
        }

        const auto it = inputManager.axisEvents.find(key);
        if (it == inputManager.axisEvents.end()) {
            return;
        }

        // TODO: find a better approach:
        // copying set of axis because it can be modified in onInputAxisEvent by user code
        // while we are iterating over it (the user should be able to modify it).
        // This should not be that bad due to the fact that a key will usually have
        // only one axis associated with it.
        // + update InputManager documentation.
        const auto axisCopy = it->second;
        for (const auto& [sAxisName, iInput] : axisCopy) {
            auto stateIt = inputManager.axisState.find(sAxisName);
            if (stateIt == inputManager.axisState.end()) {
                Logger::get().error(
                    fmt::format(
                        "input manager returned 0 "
                        "states for '{}' axis event",
                        sAxisName),
                    sGameLogCategory);
                pGameInstance->onInputAxisEvent(
                    sAxisName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0F);

                // Call on nodes that receive input.
                std::scoped_lock guard(mtxWorld.first);
                if (mtxWorld.second) {
                    const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

                    std::scoped_lock nodesGuard(pReceivingInputNodes->first);
                    for (const auto& pNode : pReceivingInputNodes->second) {
                        pNode->onInputAxisEvent(
                            sAxisName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0F);
                    }
                }

                continue;
            }

            // Mark current state.
            bool bSet = false;
            std::pair<std::vector<AxisState>, int /* last input */>& statePair = stateIt->second;
            for (auto& state : statePair.first) {
                if (iInput == 1 && state.plusKey == key) {
                    // Plus key.
                    state.bIsPlusKeyPressed = bIsPressedDown;
                    bSet = true;
                    break;
                }

                if (iInput == -1 && state.minusKey == key) {
                    // Minus key.
                    state.bIsMinusKeyPressed = bIsPressedDown;
                    bSet = true;
                    break;
                }
            }
            if (!bSet) {
                Logger::get().error(
                    fmt::format(
                        "could not find key '{}' in key "
                        "states for '{}' axis event",
                        getKeyName(key),
                        sAxisName),
                    sGameLogCategory);
                pGameInstance->onInputAxisEvent(
                    sAxisName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0F);

                // Call on nodes that receive input.
                std::scoped_lock guard(mtxWorld.first);
                if (mtxWorld.second) {
                    const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

                    std::scoped_lock nodesGuard(pReceivingInputNodes->first);
                    for (const auto& pNode : pReceivingInputNodes->second) {
                        pNode->onInputAxisEvent(
                            sAxisName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0F);
                    }
                }

                continue;
            }

            int iInputToPass = bIsPressedDown ? iInput : 0;

            if (!bIsPressedDown) {
                // See if we need to pass 0 input value.
                // See if other button are pressed.
                for (const AxisState& state : statePair.first) {
                    if (iInput == -1) {
                        // Look for plus button.
                        if (state.bIsPlusKeyPressed) {
                            iInputToPass = 1;
                            break;
                        }
                    } else {
                        // Look for minus button.
                        if (state.bIsMinusKeyPressed) {
                            iInputToPass = -1;
                            break;
                        }
                    }
                }
            }

            if (iInputToPass != statePair.second) {
                statePair.second = iInputToPass;
                pGameInstance->onInputAxisEvent(sAxisName, modifiers, static_cast<float>(iInputToPass));

                // Call on nodes that receive input.
                std::scoped_lock guard(mtxWorld.first);
                if (mtxWorld.second) {
                    const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

                    std::scoped_lock nodesGuard(pReceivingInputNodes->first);
                    for (const auto& pNode : pReceivingInputNodes->second) {
                        pNode->onInputAxisEvent(sAxisName, modifiers, static_cast<float>(iInputToPass));
                    }
                }
            }
        }
    }

    void GameManager::destroyAndCleanExistingWorld() {
        std::scoped_lock worldGuard(mtxWorld.first);

        if (mtxWorld.second == nullptr) {
            return; // nothing to do
        }

        // Make sure no GPU resource is used.
        // Block rendering.
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        // Wait for GPU to finish all work.
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Start node despawning process.
        mtxWorld.second->destroyWorld();

        // Process all `node despawned` tasks that will notify world.
        std::scoped_lock guard(mtxDeferredTasks.first); // don't accept any deferred tasks until finished
        executeDeferredTasks();

        // Can safely destroy world object now.
        mtxWorld.second = nullptr;

        // Force run GC to destroy all nodes.
        runGarbageCollection(true);

        // Make sure that all nodes were destroyed.
        const auto iAliveNodeCount = Node::getAliveNodeCount();
        if (iAliveNodeCount != 0) {
            Logger::get().error(
                fmt::format(
                    "the world was destroyed and garbage collection was finished but there are still "
                    "{} node(s) alive, here are a few reasons why this may happen:\n{}",
                    iAliveNodeCount,
                    sGcLeakReasons),
                sGameLogCategory);
        }

        // Make sure all PSOs were destroyed.
        const auto iGraphicsPsoCount = pRenderer->getPsoManager()->getCreatedGraphicsPsoCount();
        const auto iComputePsoCount = pRenderer->getPsoManager()->getCreatedComputePsoCount();
        if (iGraphicsPsoCount != 0) {
            Logger::get().error(
                fmt::format(
                    "the world was destroyed and garbage collection was finished but there are still "
                    "{} graphics PSO(s) exist",
                    iGraphicsPsoCount),
                sGameLogCategory);
        }
        if (iComputePsoCount != 0) {
            Logger::get().error(
                fmt::format(
                    "the world was destroyed and garbage collection was finished but there are still "
                    "{} compute PSO(s) exist",
                    iComputePsoCount),
                sGameLogCategory);
        }
    }
} // namespace ne
