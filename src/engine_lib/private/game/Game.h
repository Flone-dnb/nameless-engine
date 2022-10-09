﻿#pragma once

// Std.
#include <memory>
#include <functional>
#include <queue>
#include <mutex>

// Custom.
#include "game/GameInstance.h"
#include "input/InputManager.h"
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"
#include "misc/ThreadPool.h"
#include "game/World.h"

namespace ne {
    class GameInstance;
    class IRenderer;
    class Window;

    /**
     * Holds main game objects: game instance, input manager, renderer,
     * audio engine, physics engine and etc.
     *
     * Owned by Window object.
     */
    class Game {
    public:
        Game(const Game&) = delete;
        Game& operator=(const Game&) = delete;

        virtual ~Game();

        /**
         * Modifies the interval after which we need to run garbage collector again.
         * The current value can be retrieved using @ref getGarbageCollectorRunIntervalInSec.
         *
         * @remark Interval should be in range [30; 300] seconds (otherwise it will be clamped).
         *
         * @remark Note that garbage collection will also be executed additionally in some special cases,
         * such as when World is being destructed or some nodes are being detached and despawned.
         *
         * @param iGcRunIntervalInSec Interval in seconds.
         */
        void setGarbageCollectorRunInterval(long long iGcRunIntervalInSec);

        /**
         * Queues a request to run a garbage collection as a deferred task on the main thread
         * using @ref addDeferredTask.
         *
         * @remark Typically you don't need to call this function as garbage collection is executed
         * regularly (see @ref setGarbageCollectorRunInterval) but you can still call it anyway.
         *
         * @param onFinished Optional callback that will be triggered when garbage collection is finished.
         */
        void queueGarbageCollection(std::optional<std::function<void()>> onFinished);

        /**
         * Adds a function to be executed on the main thread next time @ref onBeforeNewFrame
         * is called.
         *
         * @param task Function to execute.
         *
         * @warning If you are using member functions as callbacks you need to make
         * sure that the owner object of these member functions will not be deleted until
         * this task is finished.
         */
        void addDeferredTask(const std::function<void()>& task);

        /**
         * Adds a function to be executed on the thread pool.
         *
         * TODO: std::move_only_function?
         *
         * @param task Function to execute.
         *
         * @warning If you are using member functions as callbacks you need to make
         * sure that the owner object of these member functions will not be deleted until
         * this task is finished.
         */
        void addTaskToThreadPool(const std::function<void()>& task);

        /**
         * Creates a new world with a root node to store world's node tree.
         * Replaces the old world (if existed).
         *
         * @warning This function should be called from the main thread.
         * Use @ref addDeferredTask if you are not sure.
         *
         * @param iWorldSize    Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size, maybe for the size
         * of correct physics calculations and etc. You don't need to care why we need this
         * information, you only need to know that if you leave world bounds lighting
         * or physics may be incorrect (the editor and logs should help you identify this case).
         */
        void createWorld(size_t iWorldSize = 1024);

        /**
         * Returns a pointer to world's root node.
         *
         * @return nullptr if world is not created (see @ref createWorld), otherwise world's root node.
         */
        gc<Node> getWorldRootNode();

        /**
         * Returns time since world creation (in seconds).
         *
         * @return Zero if world is not created (see @ref createWorld), otherwise time since
         * world creation (in seconds).
         */
        float getWorldTimeInSeconds();

        /**
         * Returns window that owns this object.
         *
         * @return Do not delete this pointer. Window that owns this object.
         */
        Window* getWindow() const;

        /**
         * Returns game instance that this game is using (Game owns GameInstance).
         *
         * @return Do not delete this pointer. Used game instance.
         */
        GameInstance* getGameInstance() const;

        /**
         * Returns the current interval after which we need to run garbage collector again.
         *
         * @return Interval in seconds.
         */
        long long getGarbageCollectorRunIntervalInSec();

    private:
        // The object should be created by a Window instance.
        friend class Window;

        /**
         * Constructor.
         *
         * @param pWindow Window that owns this Game object.
         */
        Game(Window* pWindow);

        /**
         * Set GameInstance derived class to react to
         * user inputs, window events and etc.
         */
        template <typename MyGameInstance>
        requires std::derived_from<MyGameInstance, GameInstance>
        void setGameInstance() {
            pGameInstance = std::make_unique<MyGameInstance>(pWindow, &inputManager);
            pGameInstance->onGameStarted();
        }

        /**
         * Called before a new frame is rendered.
         *
         * @param fTimeSincePrevCallInSec   Time in seconds that has passed since the last call
         * to this function.
         */
        void onBeforeNewFrame(float fTimeSincePrevCallInSec);

        /**
         * Called when the window (that owns this object) receives keyboard input.
         *
         * @param key            Keyboard key.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown);

        /**
         * Called when the window (that owns this object) receives mouse input.
         *
         * @param button         Mouse button.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the button down event occurred or button up.
         */
        void onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown);

        /**
         * Called when the window received mouse movement.
         *
         * @param iXOffset  Mouse X movement delta in pixels (plus if moved to the right,
         * minus if moved to the left).
         * @param iYOffset  Mouse Y movement delta in pixels (plus if moved up,
         * minus if moved down).
         */
        void onMouseMove(int iXOffset, int iYOffset) const;

        /**
         * Called when the window receives mouse scroll movement.
         *
         * @param iOffset Movement offset.
         */
        void onMouseScrollMove(int iOffset) const;

        /**
         * Called when the window focus was changed.
         *
         * @param bIsFocused  Whether the window has gained or lost the focus.
         */
        void onWindowFocusChanged(bool bIsFocused) const;

        /**
         * Called when a window that owns this game instance
         * was requested to close (no new frames will be rendered).
         * Prefer to do your destructor logic here.
         */
        void onWindowClose() const;

        /**
         * Called by the owner when a tick is fully finished.
         *
         * @warning This function should be called on the main thread.
         */
        void onTickFinished();

        /**
         * Runs garbage collection if enough time has passed since the last garbage collection
         * (see @ref setGarbageCollectorRunInterval).
         *
         * @param bForce Force run garbage collection even if the last garbage collection was run
         * not so long ago.
         *
         * @warning This function should be called on the main thread.
         */
        void runGarbageCollection(bool bForce = false);

        /** Executes all deferred tasks from @ref mtxDeferredTasks. */
        void executeDeferredTasks();

        /**
         * Triggers action events from keyboard/mouse input.
         *
         * @param key            Keyboard/mouse key.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void triggerActionEvents(
            std::variant<KeyboardKey, MouseButton> key, KeyboardModifiers modifiers, bool bIsPressedDown);

        /**
         * Triggers axis events from keyboard input.
         *
         * @param key            Keyboard key.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void triggerAxisEvents(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown);

        /** Do not delete this pointer. Window-owner of this Game. */
        Window* pWindow;

        /** Reacts to user input, window events and etc. */
        std::unique_ptr<GameInstance> pGameInstance;

        /** Game world, stores world's node tree. */
        std::pair<std::recursive_mutex, std::unique_ptr<World>> mtxWorld;

        /** Draws graphics on window. */
        std::unique_ptr<IRenderer> pRenderer;

        /** Thread pool to execute tasks. */
        ThreadPool threadPool;

        /**
         * Mutex for read/write operations on deferred tasks queue.
         * Queue of functions to call on the main thread before each frame is rendered.
         */
        std::pair<std::mutex, std::queue<std::function<void()>>> mtxDeferredTasks;

        /** Binds action/axis names with input keys. */
        InputManager inputManager;

        /** Last time we run garbage collector. */
        std::chrono::steady_clock::time_point lastGcRunTime;

        /** Interval in seconds after which we need to run garbage collector again. */
        long long iGcRunIntervalInSec = 90;

        /** ID of the main thread. */
        std::thread::id mainThreadId;

        /** Name of the category used for logging. */
        inline static const char* sGameLogCategory = "Game";
    };
} // namespace ne
