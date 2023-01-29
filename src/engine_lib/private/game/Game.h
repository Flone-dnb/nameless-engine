#pragma once

// Standard.
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
    class Renderer;
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
         * Returns last created Game object.
         *
         * @return `nullptr` if no Game object was created yet, otherwise pointer to game object.
         */
        static Game* get();

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
         * @remark Note that garbage collection will also be executed additionally in some special cases,
         * such as when World is being destructed or some nodes are being detached and despawned.
         *
         * @param onFinished Optional callback that will be triggered on the main thread
         * when garbage collection is finished (queued as @ref addDeferredTask).
         */
        void queueGarbageCollection(const std::optional<std::function<void()>>& onFinished);

        /**
         * Adds a function to be executed on the main thread next time @ref onBeforeNewFrame
         * is called.
         *
         * TODO: introduce a `std::move_only_function` overload (+ in the game instance)?
         *
         * @warning If you are using a member function as a task you need to make
         * sure that the owner object of this member function will not be deleted until
         * this task is finished.
         *
         * @remark In the task you don't need to check if the game is being destroyed,
         * the engine makes sure all tasks are finished before the game is destroyed.
         *
         * @param task Function to execute.
         *
         */
        void addDeferredTask(const std::function<void()>& task);

        /**
         * Adds a function to be executed on the thread pool.
         *
         * TODO: introduce a `std::move_only_function` overload (+ in the game instance)?
         *
         * @warning If you are using a member function as a task you need to make
         * sure that the owner object of this member function will not be deleted until
         * this task is finished.
         *
         * @remark In the task you don't need to check if the game is being destroyed,
         * the engine makes sure all tasks are finished before the game is destroyed.
         *
         * @param task Function to execute.
         */
        void addTaskToThreadPool(const std::function<void()>& task);

        /**
         * Creates a new world that contains only one node - root node.
         * Replaces the old world (if existed).
         *
         * @warning This function should be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         * Use @ref addDeferredTask if you are not sure.
         *
         * @param iWorldSize     Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size.
         * You don't need to care why we need this information, you only need to know that if
         * you leave world bounds lighting or physics may be incorrect
         * (the editor or engine will warn you if something is leaving world bounds, pay attention
         * to the logs).
         */
        void createWorld(size_t iWorldSize = 1024);

        /**
         * Loads and deserializes a node tree to be used as a new world.
         *
         * Node tree's root node will be used as world's root node.
         *
         * @warning This function should be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         * Use @ref addDeferredTask if you are not sure.
         *
         * @param pathToNodeTree Path to the file that contains a node tree to load, the ".toml"
         * extension will be automatically added if not specified.
         * @param iWorldSize     Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size.
         * You don't need to care why we need this information, you only need to know that if
         * you leave world bounds lighting or physics may be incorrect
         * (the editor or engine will warn you if something is leaving world bounds, pay attention
         * to the logs).
         *
         * @return Error if failed to deserialize the node tree.
         */
        std::optional<Error>
        loadNodeTreeAsWorld(const std::filesystem::path& pathToNodeTree, size_t iWorldSize = 1024);

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
         * Returns world size in game units.
         *
         * @return World size.
         */
        size_t getWorldSize();

        /**
         * Returns the current amount of spawned nodes that are marked as "should be called every frame".
         *
         * @return Amount of spawned nodes that should be called every frame.
         */
        size_t getCalledEveryFrameNodeCount();

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
        long long getGarbageCollectorRunIntervalInSec() const;

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
         * Contains destructor logic: runs GC for the last time, destroys game instance, etc.
         *
         * @warning Should be called from the main thread.
         *
         * @remark Can be safely called multiple times (additional calls will be ignored).
         *
         * @remark Can be used to destroy the game without clearing the actual game pointer.
         */
        void destroy();

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
        void onMouseMove(int iXOffset, int iYOffset);

        /**
         * Called when the window receives mouse scroll movement.
         *
         * @param iOffset Movement offset.
         */
        void onMouseScrollMove(int iOffset);

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
         * @warning This function should be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         *
         * @param bForce Force run garbage collection even if the last garbage collection was run
         * not so long ago.
         *
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

        /**
         * Destroys the current world (if exists) and runs GC to clean everything up.
         *
         * @warning This function should be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         */
        void destroyAndCleanExistingWorld();

        /** Do not delete this pointer. Window-owner of this Game. */
        Window* pWindow;

        /** Reacts to user input, window events and etc. */
        std::unique_ptr<GameInstance> pGameInstance;

        /** Game world, stores world's node tree. */
        std::pair<std::recursive_mutex, std::unique_ptr<World>> mtxWorld;

        /** Draws graphics on window. */
        std::unique_ptr<Renderer> pRenderer;

        /** Thread pool to execute tasks. */
        ThreadPool threadPool;

        /**
         * Mutex for read/write operations on deferred tasks queue.
         * Queue of functions to call on the main thread before each frame is rendered.
         */
        std::pair<std::recursive_mutex, std::queue<std::function<void()>>> mtxDeferredTasks;

        /** Binds action/axis names with input keys. */
        InputManager inputManager;

        /** Last time we run garbage collector. */
        std::chrono::steady_clock::time_point lastGcRunTime;

        /** Interval in seconds after which we need to run garbage collector again. */
        long long iGcRunIntervalInSec = 120;

        /** ID of the main thread. */
        std::thread::id mainThreadId;

        /** Whether @ref destroy was called or not. */
        bool bIsDestroyed = false;

        /** Name of the category used for logging. */
        inline static const char* sGameLogCategory = "Game";

        /** Description of reasons why a leak may occur. */
        inline static const char* sGcLeakReasons =
            "1. you are not using STL container wrappers for gc "
            "pointers (for example, you need to use `gc_vector<T>` instead of `std::vector<gc<T>>`, "
            "and other `gc_*` containers when storing gc pointers),\n"
            "2. you are capturing `gc` pointer(s) in `std::function` (this might leak in some special "
            "cases, such as when you have a class `MyClass` with a `std::function` member in it "
            "and you capture a `gc<MyClass>` in this `std::function` member which creates a non-resolvable "
            "cycle, for such cases use `gc_function` instead of `std::function` as a member of your class).";
    };
} // namespace ne
