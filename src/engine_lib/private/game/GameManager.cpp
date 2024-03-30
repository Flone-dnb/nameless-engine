#include "GameManager.h"

// Standard.
#include <thread>
#include <sstream>
#include <format>

// Custom.
#include "io/Logger.h"
#include "io/Serializable.h"
#include "misc/ProjectPaths.h"
#include "render/Renderer.h"
#include "material/Material.h"
#include "shader/general/Shader.h"
#include "render/general/pipeline/PipelineManager.h"
#include "io/FieldSerializerManager.h"
#include "game/camera/CameraManager.h"
#include "shader/ShaderManager.h"
#include "game/nodes/Node.h"
#include "misc/Profiler.hpp"
#include "GcInfoCallbacks.hpp"

namespace ne {
    // Static pointer for accessing last created game manager.
    static GameManager* pLastCreatedGameManager = nullptr;

    GameManager::GameManager(Window* pWindow) {
        // Bind GC info callbacks.
        sgc::GcInfoCallbacks::setCallbacks(
            [](const char* pWarningMessage) { Logger::get().warn(pWarningMessage); },
            [](const char* pCriticalErrorMessage) {
                Error error(pCriticalErrorMessage);
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            });

        // Save window.
        this->pWindow = pWindow;

        // Update static pointer.
        Logger::get().info("new GameManager is created, updating static GameManager pointer");
        pLastCreatedGameManager = this;

#if defined(ENABLE_PROFILER)
        // Initialize profiler.
        Profiler::initialize();
        rmt_SetCurrentThreadName("Main Thread");
        Logger::get().warn("profiler enabled, it may cause freezes and memory leaks (remember to turn off "
                           "when finished profiling)");
#endif

        // Log build mode.
#if defined(DEBUG)
        Logger::get().info("DEBUG macro is defined, running DEBUG build");
#else
        Logger::get().info("DEBUG macro is not defined, running RELEASE build");
#endif
    }

    std::optional<Error> GameManager::initialize(std::optional<RendererType> preferredRenderer) {
        if (bIsInitialized) [[unlikely]] {
            return Error("already initialized");
        }

        // Make sure that resources directory is set and exists.
        const auto pathToRes = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT);
        if (!std::filesystem::exists(pathToRes)) [[unlikely]] {
            return Error(std::format("expected resources directory to exist at \"{}\"", pathToRes.string()));
        }

        // Save ID of this thread (should be main thread).
        mainThreadId = std::this_thread::get_id();

        // Register engine serializers.
        FieldSerializerManager::registerEngineFieldSerializers();

        // Setup initial GC time.
        lastGcRunTime = std::chrono::steady_clock::now();
        Logger::get().info(std::format("garbage collector will run every {} seconds", iGcRunIntervalInSec));

#if defined(DEBUG)
        SerializableObjectFieldSerializer::checkGuidUniqueness();
#endif

        // Create renderer.
        auto result = Renderer::create(this, preferredRenderer);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pRenderer = std::get<std::unique_ptr<Renderer>>(std::move(result));

        // Mark as initialized (because renderer was created successfully).
        bIsInitialized = true;

        // Create camera manager.
        pCameraManager = std::make_unique<CameraManager>(pRenderer.get());

        return {};
    }

    void GameManager::destroy() {
        if (bIsBeingDestroyed || !bIsInitialized) {
            return;
        }
        bIsBeingDestroyed = true;

        // Log destruction so that it will be slightly easier to read logs.
        Logger::get().info("\n\n");
        Logger::get().info("starting game manager destruction...");
        Logger::get().flushToDisk();

        // Wait for GPU to finish all work.
        // Make sure no GPU resource is used.
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Destroy world if it exists.
        {
            std::scoped_lock guard(mtxWorld.first);

            if (mtxWorld.second != nullptr) {
                // Destroy world before game instance, so that no node will reference game instance.
                // (causes every node to queue a deferred task for despawning from world)
                mtxWorld.second->destroyWorld();

                // Don't accept any new deferred tasks anymore.
                bShouldAcceptNewDeferredTasks = false;

                // Process all "node despawned" tasks that notify the world.
                executeDeferredTasks();

                // Can safely destroy the world object now.
                mtxWorld.second = nullptr;
            }
        }

        // Make sure the thread pool and all deferred tasks are finished.
        threadPool.stop(); // they might queue new deferred tasks
        bShouldAcceptNewDeferredTasks = false;
        executeDeferredTasks();

        {
            // Self check: make sure no thread pool task/deferred task created a new world.
            std::scoped_lock guard(mtxWorld.first);
            if (mtxWorld.second != nullptr) [[unlikely]] {
                Error error("game is being destroyed and the last thread pool / deferred tasks were executed "
                            "but some task has created a new world");
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        }

        // Explicitly destroy the game instance before running the GC so that if the game instance holds
        // any GC pointers they will be cleared/removed.
        pGameInstance = nullptr;

        // Run the GC for the last time.
        Logger::get().info("GameManager is being destroyed, running garbage collector...");
        Logger::get().flushToDisk(); // flush to disk in order to see if a crash was caused by a GC

        const auto iFreedObjectCount = sgc::GarbageCollector::get().collectGarbage();
        const auto iGcAllocationsAlive = sgc::GarbageCollector::get().getAliveAllocationCount();

        // Log results.
        Logger::get().info(std::format(
            "garbage collector has finished: "
            "deleted {} allocation(s) ({} alive)",
            iFreedObjectCount,
            iGcAllocationsAlive));

        // Make sure there are no nodes alive.
        const auto iNodesAlive = Node::getAliveNodeCount();
        if (iNodesAlive != 0) [[unlikely]] {
            Logger::get().error(std::format(
                "the game was destroyed and a full garbage collection was run but there are still "
                "{} node(s) alive, here are a few reasons why this may happen:\n{}",
                iNodesAlive,
                sGcLeakReasons));
        }

        // Make sure there are no GC allocations alive.
        if (iGcAllocationsAlive != 0) [[unlikely]] {
            Logger::get().error(std::format(
                "the game was destroyed and a full garbage collection was run but there are still "
                "{} allocation(s) alive, here are a few reasons why this may happen:\n{}",
                iGcAllocationsAlive,
                sGcLeakReasons));
        }

        // ONLY AFTER THE GC has finished, all tasks were finished and all nodes were deleted.
        // Clear global game pointer.
        Logger::get().info("clearing static GameManager pointer");
        pLastCreatedGameManager = nullptr;

        // Explicitly destroy the renderer to check how much shaders left in the memory.
        pRenderer = nullptr;
        Logger::get().info("renderer is destroyed");

        // Make sure there are no shaders left in memory.
        const auto iTotalShadersInMemory = Shader::getCurrentAmountOfShadersInMemory();
        if (iTotalShadersInMemory != 0) [[unlikely]] {
            Logger::get().error(std::format(
                "the renderer was destroyed but there are still {} shader(s) left in the memory",
                iTotalShadersInMemory));
        }

        // Make sure there are no materials exist.
        const auto iTotalMaterialCount = Material::getCurrentAliveMaterialCount();
        if (iTotalMaterialCount != 0) [[unlikely]] {
            Logger::get().error(std::format(
                "the game was destroyed but there are still {} material(s) alive", iTotalMaterialCount));
        }
    }

    void GameManager::onGameStarted() {
        // Make sure game instance was created.
        if (pGameInstance == nullptr) [[unlikely]] {
            Error error("expected game instance to exist at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Log game start so that it will be slightly easier to read logs.
        Logger::get().info("\n\n");
        Logger::get().info("game started");
        Logger::get().flushToDisk();

        pGameInstance->onGameStarted();
    }

    void GameManager::onTickFinished() {
        PROFILE_FUNC;

        runGarbageCollection();
    }

    void GameManager::runGarbageCollection(bool bForce) {
        // Make sure this function is being executed on the main thread.
        const auto currentThreadId = std::this_thread::get_id();
        if (currentThreadId != mainThreadId) [[unlikely]] {
            std::stringstream currentThreadIdString;
            currentThreadIdString << currentThreadId;

            std::stringstream mainThreadIdString;
            mainThreadIdString << mainThreadId;

            Error err(std::format(
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

        PROFILE_FUNC;

        // Finish all deferred tasks and lock them until not finished with garbage collector.
        // We want to finish all deferred tasks right now because there might be
        // node member functions waiting to be executed - execute them and only
        // then delete nodes.
        executeDeferredTasks();

        // Log start.
        Logger::get().info("running garbage collector...");
        Logger::get().flushToDisk(); // flush to disk in order to see if a crash was caused by a GC

        // Save time to measure later.
        const auto startTime = std::chrono::steady_clock::now();

        // Run garbage collector.
        const auto iFreedObjectCount = sgc::GarbageCollector::get().collectGarbage();
        const auto iGcAllocationsAlive = sgc::GarbageCollector::get().getAliveAllocationCount();

        // Measure the time it took to run garbage collector.
        const auto endTime = std::chrono::steady_clock::now();
        const auto timeTookInMs =
            std::chrono::duration<float, std::chrono::milliseconds::period>(endTime - startTime).count();

        // Log results.
        Logger::get().info(std::format(
            "garbage collector has finished, took {:.1F} millisecond(s): "
            "deleted {} allocation(s) ({} alive)",
            timeTookInMs,
            iFreedObjectCount,
            iGcAllocationsAlive));

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
        PROFILE_FUNC;

        // Save delta time.
        timeSincePrevFrameInSec = timeSincePrevCallInSec;

        // Call on game instance.
        pGameInstance->onBeforeNewFrame(timeSincePrevCallInSec);

        {
            PROFILE_SCOPE(TriggerOnNodes);

            // Call on all tickable nodes.
            std::scoped_lock guard(mtxWorld.first);
            if (mtxWorld.second == nullptr) {
                return;
            }

            const auto pCalledEveryFrameNodes = mtxWorld.second->getCalledEveryFrameNodes();

            auto callTick = [&](std::pair<std::recursive_mutex, std::unordered_set<Node*>>* pTickGroup) {
                std::scoped_lock nodesGuard(pTickGroup->first);

                std::unordered_set<Node*>* pNodes = &pTickGroup->second;
                for (auto it = pNodes->begin(); it != pNodes->end(); ++it) {
                    (*it)->onBeforeNewFrame(timeSincePrevCallInSec);
                }
            };

            callTick(&pCalledEveryFrameNodes->mtxFirstTickGroup);
            callTick(&pCalledEveryFrameNodes->mtxSecondTickGroup);
        }
    }

    void GameManager::executeDeferredTasks() {
        // We have to guarantee that the GameManager exists while deferred tasks are running.
        if (pLastCreatedGameManager == nullptr) [[unlikely]] {
            Error error(
                "unable to execute deferred tasks while the GameManager is invalid (this is an engine bug)");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        bool bExecutedAtLeastOneTask = false;

        do {
            bExecutedAtLeastOneTask = false;
            do {
                std::function<void()> task;

                {
                    std::scoped_lock guard(mtxDeferredTasks.first);

                    if (mtxDeferredTasks.second.empty()) {
                        break;
                    }

                    task = std::move(mtxDeferredTasks.second.front());
                    mtxDeferredTasks.second.pop(); // remove first and only then execute to avoid recursion
                }

                // Execute the task while the mutex is not locked to avoid a deadlock,
                // because inside of this task the user might want to interact with some other thread
                // which also might want to add a deferred task.
                task();
                bExecutedAtLeastOneTask = true;
            } while (true); // TODO: rework this

            // Check if the deferred tasks that we executed added more deferred tasks.
        } while (bExecutedAtLeastOneTask);
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
        addDeferredTask([this, onCreated, iWorldSize]() {
            std::scoped_lock guard(mtxWorld.first);

            destroyAndCleanExistingWorld();

            // Don't create new world if the game is being destroyed.
            if (bIsBeingDestroyed) {
                return;
            }

            mtxWorld.second = World::createWorld(this, iWorldSize);

            onCreated({});
        });
    }

    void GameManager::loadNodeTreeAsWorld(
        const std::function<void(const std::optional<Error>&)>& onLoaded,
        const std::filesystem::path& pathToNodeTree,
        size_t iWorldSize) {
        addDeferredTask([this, onLoaded, pathToNodeTree, iWorldSize]() {
            std::scoped_lock guard(mtxWorld.first);

            destroyAndCleanExistingWorld();

            // Don't create new world if the game is being destroyed.
            if (bIsBeingDestroyed) {
                return;
            }

            // Load new world.
            auto result = World::loadNodeTreeAsWorld(this, pathToNodeTree, iWorldSize);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(result);
                error.addCurrentLocationToErrorStack();
                onLoaded(error);
                return;
            }
            mtxWorld.second = std::get<std::unique_ptr<World>>(std::move(result));

            // After new root node is spawned run garbage collection to make sure there is no garbage.
            runGarbageCollection(true);

            // Run shader manager self validation.
            pRenderer->getShaderManager()->performSelfValidation();

            onLoaded({});
        });
    }

    sgc::GcPtr<Node> GameManager::getWorldRootNode() {
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
        PROFILE_FUNC;

        // Trigger raw (no events) input processing function.
        pGameInstance->onKeyboardInput(key, modifiers, bIsPressedDown);

        // Trigger input events.
        triggerActionEvents(key, modifiers, bIsPressedDown);
        triggerAxisEvents(key, modifiers, bIsPressedDown);
    }

    void GameManager::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {
        PROFILE_FUNC;

        // Trigger raw (no events) input processing function.
        pGameInstance->onMouseInput(button, modifiers, bIsPressedDown);

        // Trigger input events.
        triggerActionEvents(button, modifiers, bIsPressedDown);
    }

    void GameManager::onMouseMove(double xOffset, double yOffset) {
        PROFILE_FUNC;

        // Trigger game instance logic.
        pGameInstance->onMouseMove(xOffset, yOffset);

        // Call on nodes that receive input.
        std::scoped_lock guard(mtxWorld.first);
        if (mtxWorld.second) {
            const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

            std::scoped_lock nodesGuard(pReceivingInputNodes->first);
            std::unordered_set<Node*>* pNodes = &pReceivingInputNodes->second;
            for (auto it = pNodes->begin(); it != pNodes->end(); ++it) {
                (*it)->onMouseMove(xOffset, yOffset);
            }
        }
    }

    void GameManager::onMouseScrollMove(int iOffset) {
        PROFILE_FUNC;

        // Trigger game instance logic.
        pGameInstance->onMouseScrollMove(iOffset);

        // Call on nodes that receive input.
        std::scoped_lock guard(mtxWorld.first);
        if (mtxWorld.second) {
            const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

            std::scoped_lock nodesGuard(pReceivingInputNodes->first);
            std::unordered_set<Node*>* pNodes = &pReceivingInputNodes->second;
            for (auto it = pNodes->begin(); it != pNodes->end(); ++it) {
                (*it)->onMouseScrollMove(iOffset);
            }
        }
    }

    void GameManager::onWindowFocusChanged(bool bIsFocused) const {
        pGameInstance->onWindowFocusChanged(bIsFocused);
    }

    void GameManager::onFramebufferSizeChanged(int iWidth, int iHeight) const {
        pGameInstance->onFramebufferSizeChanged(iWidth, iHeight);
    }

    void GameManager::onWindowClose() const {
        pGameInstance->stopAndDisableCreatedTimers();
        pGameInstance->onWindowClose();
    }

    void GameManager::addDeferredTask(const std::function<void()>& task) {
        if (!bShouldAcceptNewDeferredTasks) {
            // Don't queue any more tasks.
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

    bool GameManager::isNodeSpawned(size_t iNodeId) {
        std::scoped_lock guard(mtxWorld.first);
        if (mtxWorld.second == nullptr) {
            return false;
        }

        return mtxWorld.second->isNodeSpawned(iNodeId);
    }

    bool GameManager::isBeingDestroyed() const { return bIsBeingDestroyed; }

    void GameManager::triggerActionEvents(
        std::variant<KeyboardKey, MouseButton> key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        PROFILE_FUNC;

        std::scoped_lock guard(inputManager.mtxActionEvents);

        // Make sure this key is registered in some action.
        const auto it = inputManager.actionEvents.find(key);
        if (it == inputManager.actionEvents.end()) {
            return;
        }

        // Copying array of actions because it can be modified in `onInputActionEvent` by user code
        // while we are iterating over it (the user should be able to modify it).
        // This should not be that bad due to the fact that action events is just a small array of ints.
        const auto vActionIds = it->second;
        for (const auto& iActionId : vActionIds) {
            // Get state of the action.
            const auto actionStateIt = inputManager.actionState.find(iActionId);
            if (actionStateIt == inputManager.actionState.end()) [[unlikely]] {
                // Unexpected, nothing to process.
                Logger::get().error(
                    std::format("input manager returned 0 states for action event with ID {}", iActionId));
                continue;
            }

            // Stores various keys that can activate this action (for example we can have
            // buttons W and ArrowUp activating the same event named "moveForward").
            std::pair<std::vector<ActionState>, bool /* action state */>& actionStatePair =
                actionStateIt->second;

            // Find an action key that matches the received one.
            bool bFoundKey = false;
            for (auto& actionKey : actionStatePair.first) {
                if (actionKey.key == key) {
                    // Mark key's state.
                    actionKey.bIsPressed = bIsPressedDown;
                    bFoundKey = true;
                    break;
                }
            }

            // Log an error if the key is not found.
            if (!bFoundKey) [[unlikely]] {
                if (std::holds_alternative<KeyboardKey>(key)) {
                    Logger::get().error(std::format(
                        "could not find the key `{}` in key states for action event with ID {}",
                        getKeyName(std::get<KeyboardKey>(key)),
                        iActionId));
                } else {
                    Logger::get().error(std::format(
                        "could not find mouse button `{}` in key states for action event with ID {}",
                        static_cast<int>(std::get<MouseButton>(key)),
                        iActionId));
                }
            }

            // Save received key state as action state.
            bool bNewActionState = bIsPressedDown;

            if (!bIsPressedDown) {
                // The key is not pressed but this does not mean that we need to broadcast
                // a notification about changed state. See if other button are pressed.
                for (const auto actionKey : actionStatePair.first) {
                    if (actionKey.bIsPressed) {
                        // Save action state.
                        bNewActionState = true;
                        break;
                    }
                }
            }

            // See if action state is changed.
            if (bNewActionState == actionStatePair.second) {
                continue;
            }

            // Action state was changed, notify the game.

            // Save new action state.
            actionStatePair.second = bNewActionState;

            // Notify game instance.
            pGameInstance->onInputActionEvent(iActionId, modifiers, bNewActionState);

            // Notify nodes that receive input.
            std::scoped_lock guard(mtxWorld.first);
            if (mtxWorld.second != nullptr) {
                const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

                std::scoped_lock nodesGuard(pReceivingInputNodes->first);
                for (const auto& pNode : pReceivingInputNodes->second) {
                    pNode->onInputActionEvent(iActionId, modifiers, bNewActionState);
                }
            }
        }
    }

    void GameManager::triggerAxisEvents(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        PROFILE_FUNC;

        std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxAxisEvents);

        // Make sure this key is registered in some axis event.
        const auto it = inputManager.axisEvents.find(key);
        if (it == inputManager.axisEvents.end()) {
            return;
        }

        // Copying array of axis events because it can be modified in `onInputAxisEvent` by user code
        // while we are iterating over it (the user should be able to modify it).
        // This should not be that bad due to the fact that axis events is just a small array of ints.
        const auto axisCopy = it->second;
        for (const auto& [iAxisEventId, bTriggersPlusInput] : axisCopy) {
            // Get state of the action.
            auto axisStateIt = inputManager.axisState.find(iAxisEventId);
            if (axisStateIt == inputManager.axisState.end()) [[unlikely]] {
                // Unexpected.
                Logger::get().error(
                    std::format("input manager returned 0 states for axis event with ID {}", iAxisEventId));
                continue;
            }

            // Stores various keys that can activate this action (for example we can have
            // buttons W and ArrowUp activating the same event named "moveForward").
            std::pair<std::vector<AxisState>, int /* last input */>& axisStatePair = axisStateIt->second;

            // Find an action key that matches the received one.
            bool bFound = false;
            for (auto& state : axisStatePair.first) {
                if (bTriggersPlusInput && state.plusKey == key) {
                    // Found it, it's registered as plus key.
                    // Mark key's state.
                    state.bIsPlusKeyPressed = bIsPressedDown;
                    bFound = true;
                    break;
                }

                if (!bTriggersPlusInput && state.minusKey == key) {
                    // Found it, it's registered as minus key.
                    // Mark key's state.
                    state.bIsMinusKeyPressed = bIsPressedDown;
                    bFound = true;
                    break;
                }
            }

            // Log an error if the key is not found.
            if (!bFound) [[unlikely]] {
                Logger::get().error(std::format(
                    "could not find key `{}` in key states for axis event with ID {}",
                    getKeyName(key),
                    iAxisEventId));
                continue;
            }

            // Save action's state as key state.
            int iAxisInputState = 0;
            if (bIsPressedDown) {
                iAxisInputState = bTriggersPlusInput ? 1 : -1;
            }

            if (!bIsPressedDown) {
                // The key is not pressed but this does not mean that we need to broadcast
                // a notification about state being equal to 0. See if other button are pressed.
                for (const AxisState& state : axisStatePair.first) {
                    if (!bTriggersPlusInput && state.bIsPlusKeyPressed) { // Look for plus button.
                        iAxisInputState = 1;
                        break;
                    }

                    if (bTriggersPlusInput && state.bIsMinusKeyPressed) { // Look for minus button.
                        iAxisInputState = -1;
                        break;
                    }
                }
            }

            // See if axis state is changed.
            if (iAxisInputState == axisStatePair.second) {
                continue;
            }

            // Axis state was changed, notify the game.

            // Save new axis state.
            axisStatePair.second = iAxisInputState;

            // Notify game instance.
            pGameInstance->onInputAxisEvent(iAxisEventId, modifiers, static_cast<float>(iAxisInputState));

            // Notify nodes that receive input.
            std::scoped_lock guard(mtxWorld.first);
            if (mtxWorld.second != nullptr) {
                const auto pReceivingInputNodes = mtxWorld.second->getReceivingInputNodes();

                std::scoped_lock nodesGuard(pReceivingInputNodes->first);
                for (const auto& pNode : pReceivingInputNodes->second) {
                    pNode->onInputAxisEvent(iAxisEventId, modifiers, static_cast<float>(iAxisInputState));
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

        // Process all "node despawned" tasks that will notify world.
        executeDeferredTasks();

        // Can safely destroy world object now.
        mtxWorld.second = nullptr;

        // Force run GC to destroy all nodes.
        runGarbageCollection(true);

        // Make sure that all nodes were destroyed.
        const auto iAliveNodeCount = Node::getAliveNodeCount();
        if (iAliveNodeCount != 0) {
            Logger::get().error(std::format(
                "the world was destroyed and garbage collection was finished but there are still "
                "{} node(s) alive, here are a few reasons why this may happen:\n{}",
                iAliveNodeCount,
                sGcLeakReasons));
        }

        // Make sure all pipelines were destroyed.
        const auto iGraphicsPipelineCount =
            pRenderer->getPipelineManager()->getCurrentGraphicsPipelineCount();
        if (iGraphicsPipelineCount != 0) {
            Logger::get().error(std::format(
                "the world was destroyed and garbage collection was finished but there are still "
                "{} graphics pipelines(s) exist",
                iGraphicsPipelineCount));
        }
    }
} // namespace ne
