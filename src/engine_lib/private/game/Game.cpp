#include "Game.h"

// STL.
#include <thread>
#include <sstream>

// Custom.
#include "io/Logger.h"
#include "io/Serializable.h"
#include "misc/ProjectPaths.h"
#include "misc/GC.hpp"
#include "render/Renderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif
#include "io/serializers/PrimitiveFieldSerializer.h"
#include "io/serializers/StringFieldSerializer.h"
#include "io/serializers/VectorFieldSerializer.h"
#include "io/serializers/UnorderedMapFieldSerializer.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"

// External.
#include "fmt/core.h"

namespace ne {
    Game::Game(Window* pWindow) {
        this->pWindow = pWindow;

        // Make sure that `res` directory is set and exists.
        // (intentionally ignore result, will show an error if not exists)
        ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT);

        // Save ID to this thread (should be main thread).
        mainThreadId = std::this_thread::get_id();

        // Add engine serializers.
        Serializable::addFieldSerializer(std::make_unique<PrimitiveFieldSerializer>());
        Serializable::addFieldSerializer(std::make_unique<StringFieldSerializer>());
        Serializable::addFieldSerializer(std::make_unique<VectorFieldSerializer>());
        Serializable::addFieldSerializer(std::make_unique<UnorderedMapFieldSerializer>());
        Serializable::addFieldSerializer(std::make_unique<SerializableObjectFieldSerializer>());

        // Mark start time.
        gc_collector()->collect(); // run for the first time to setup things (I guess)
        lastGcRunTime = std::chrono::steady_clock::now();
        Logger::get().info(
            fmt::format("garbage collector will run every {} seconds", iGcRunIntervalInSec),
            sGameLogCategory);

#if defined(DEBUG)
        Serializable::checkGuidUniqueness();
#endif

#if defined(WIN32)
        pRenderer = std::make_unique<DirectXRenderer>(this);
#elif __linux__
        throw std::runtime_error("No renderer for this platform.");
#else
        throw std::runtime_error("No renderer for this platform.");
#endif
    }

    void Game::onTickFinished() { runGarbageCollection(); }

    void Game::runGarbageCollection(bool bForce) {
        if (!bForce) {
            // Check if we need to run garbage collector.
            const auto iTimeSinceLastGcInSec = std::chrono::duration_cast<std::chrono::seconds>(
                                                   std::chrono::steady_clock::now() - lastGcRunTime)
                                                   .count();
            if (iTimeSinceLastGcInSec < iGcRunIntervalInSec) {
                return;
            }
        }

        // Run garbage collector.
        Logger::get().info("running garbage collector...", sGarbageCollectorLogCategory);

        // Measure the time it takes to run garbage collector.
        const auto start = std::chrono::steady_clock::now();
        gc_collector()->collect();
        const auto end = std::chrono::steady_clock::now();
        const auto durationInMs =
            static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) *
            0.000001f;

        // Limit precision to 1 digit.
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

        lastGcRunTime = std::chrono::steady_clock::now();
    }

    Game::~Game() {
        threadPool.stop();

        {
            std::scoped_lock guard(mtxWorld.first);

            if (mtxWorld.second) {
                // Explicitly destroy world before game instance, so that no node
                // will reference game instance.
                mtxWorld.second->destroyWorld(); // despawns all nodes and removes pointer to root node

                // At this point (when no node is spawned) nodes will not be able to
                // access game instance or world so we can safely destroy world.
                mtxWorld.second = nullptr;
            }
        }

        // Run GC for the last time.
        Logger::get().info("game is destroyed, running garbage collector...", sGarbageCollectorLogCategory);
        gc_collector()->fullCollect();

        // Log results.
        Logger::get().info(
            fmt::format(
                "garbage collector has finished: "
                "freed {} object(s) ({} left alive)",
                gc_collector()->getLastFreedObjectsCount(),
                gc_collector()->getAliveObjectsCount()),
            sGarbageCollectorLogCategory);

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
    }

    void Game::setGarbageCollectorRunInterval(long long iGcRunIntervalInSec) {
        this->iGcRunIntervalInSec = std::clamp<long long>(iGcRunIntervalInSec, 30, 300);
    }

    void Game::queueGarbageCollection(std::optional<std::function<void()>> onFinished) {
        addDeferredTask([this, onFinished]() {
            runGarbageCollection(true);
            if (onFinished.has_value()) {
                onFinished->operator()();
            }
        });
    }

    void Game::onBeforeNewFrame(float fTimeSincePrevCallInSec) {
        executeDeferredTasks();

        pRenderer->getShaderManager()->performSelfValidation();

        pGameInstance->onBeforeNewFrame(fTimeSincePrevCallInSec);
    }

    void Game::executeDeferredTasks() {
        std::queue<std::function<void()>> localTasks;
        {
            std::scoped_lock<std::mutex> guard(mtxDeferredTasks.first);
            if (mtxDeferredTasks.second.empty()) {
                return;
            }

            localTasks = std::move(mtxDeferredTasks.second);
            mtxDeferredTasks.second = {};
        }
        while (!localTasks.empty()) {
            localTasks.front()();
            localTasks.pop();
        }
    }

    void Game::addTaskToThreadPool(const std::function<void()>& task) { threadPool.addTask(task); }

    void Game::createWorld(size_t iWorldSize) {
        const auto currentThreadId = std::this_thread::get_id();
        if (currentThreadId != mainThreadId) {
            std::stringstream currentThreadIdString;
            currentThreadIdString << currentThreadId;

            std::stringstream mainThreadIdString;
            mainThreadIdString << mainThreadId;

            Error err(fmt::format(
                "An attempt was made to create a world in a non main thread "
                "(main thread ID: {}, current thread ID: {})",
                mainThreadIdString.str(),
                currentThreadIdString.str()));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        std::scoped_lock guard(mtxWorld.first);

        if (mtxWorld.second) {
            // Explicitly destroy world before setting nullptr, so that no node will reference the world.
            mtxWorld.second->destroyWorld();

            // At this point (when no node is spawned) nodes will not be able to
            // access game instance or world so we can safely destroy world.
            mtxWorld.second = nullptr;

            // Force run GC to destroy all nodes.
            runGarbageCollection(true);

            // Check that all nodes were destroyed.
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
        }

        mtxWorld.second = World::createWorld(this, iWorldSize);
    }

    gc<Node> Game::getWorldRootNode() {
        std::scoped_lock guard(mtxWorld.first);

        if (!mtxWorld.second)
            return nullptr;

        return mtxWorld.second->getRootNode();
    }

    float Game::getWorldTimeInSeconds() {
        std::scoped_lock guard(mtxWorld.first);

        if (!mtxWorld.second)
            return 0.0f;

        return mtxWorld.second->getWorldTimeInSeconds();
    }

    void Game::onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onKeyboardInput(key, modifiers, bIsPressedDown);

        triggerActionEvents(key, modifiers, bIsPressedDown);

        triggerAxisEvents(key, modifiers, bIsPressedDown);
    }

    void Game::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onMouseInput(button, modifiers, bIsPressedDown);

        triggerActionEvents(button, modifiers, bIsPressedDown);
    }

    void Game::onMouseMove(int iXOffset, int iYOffset) const {
        pGameInstance->onMouseMove(iXOffset, iYOffset);
    }

    void Game::onMouseScrollMove(int iOffset) const { pGameInstance->onMouseScrollMove(iOffset); }

    void Game::onWindowFocusChanged(bool bIsFocused) const {
        pGameInstance->onWindowFocusChanged(bIsFocused);
    }

    void Game::onWindowClose() const { pGameInstance->onWindowClose(); }

    void Game::addDeferredTask(const std::function<void()>& task) {
        {
            std::scoped_lock guard(mtxDeferredTasks.first);
            mtxDeferredTasks.second.push(task);
        }

        if (!pGameInstance) {
            // Tick not started yet but we already have some tasks (probably engine internal calls).
            // Execute them now.
            executeDeferredTasks();
        }
    }

    Window* Game::getWindow() const { return pWindow; }

    GameInstance* Game::getGameInstance() const { return pGameInstance.get(); }

    long long Game::getGarbageCollectorRunIntervalInSec() { return iGcRunIntervalInSec; }

    void Game::triggerActionEvents(
        std::variant<KeyboardKey, MouseButton> key, KeyboardModifiers modifiers, bool bIsPressedDown) {
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

                if (bSet == false) {
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

                if (bIsPressedDown == false) {
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
                }
            }
        }
    }

    void Game::triggerAxisEvents(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
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
                    sAxisName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0f);
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
                } else if (iInput == -1 && state.minusKey == key) {
                    // Minus key.
                    state.bIsMinusKeyPressed = bIsPressedDown;
                    bSet = true;
                    break;
                }
            }
            if (bSet == false) {
                Logger::get().error(
                    fmt::format(
                        "could not find key '{}' in key "
                        "states for '{}' axis event",
                        getKeyName(key),
                        sAxisName),
                    sGameLogCategory);
                pGameInstance->onInputAxisEvent(
                    sAxisName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0f);
                continue;
            }

            int iInputToPass = bIsPressedDown ? iInput : 0;

            if (bIsPressedDown == false) {
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
            }
        }
    }
} // namespace ne
