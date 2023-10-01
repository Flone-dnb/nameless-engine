#pragma once

// Standard.
#include <functional>
#include <optional>
#include <mutex>
#include <filesystem>

// Custom.
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"
#include "misc/Timer.h"
#include "misc/GC.hpp"

namespace ne {
    class Window;
    class InputManager;
    class GameManager;
    class Node;
    class CameraManager;

    /**
     * Main game class, exists while the game window is not closed
     * (i.e. while the game is not closed).
     *
     * Reacts to user inputs, window events and etc.
     * Owned by Game object.
     */
    class GameInstance {
        // Game will trigger input events.
        friend class GameManager;

    public:
        GameInstance() = delete;

        /**
         * Constructor.
         *
         * @remark There is no need to save window/input manager pointers
         * in derived classes as the base class already saves these pointers and
         * provides @ref getWindow and @ref getInputManager functions.
         *
         * @param pGameWindow   Window that owns this game instance.
         * @param pGameManager  GameManager that owns this game instance.
         * @param pInputManager Input manager of the owner Game object.
         */
        GameInstance(Window* pGameWindow, GameManager* pGameManager, InputManager* pInputManager);

        GameInstance(const GameInstance&) = delete;
        GameInstance& operator=(const GameInstance&) = delete;

        virtual ~GameInstance() = default;

        /**
         * Returns the time in seconds that has passed since the very first window was created.
         *
         * @return Time in seconds.
         */
        static float getTotalApplicationTimeInSec();

        /**
         * Adds a function to be executed on the main thread next time @ref onBeforeNewFrame
         * is called.
         *
         * @warning Don't capture `gc` pointers in `std::function`, this is not supported and
         * will cause memory leaks/crashes!
         *
         * @warning If you are using member functions/fields inside of the task you need to make
         * sure that the owner object of these member functions/fields will not be deleted until
         * this task is finished. If you use GameInstance or Node member functions/fields inside of
         * the task and submitting a deferred tasks from the main thread then
         * ignore this warning, they are safe to use in deferred tasks and will not be
         * deleted until the task is finished. If you are submitting a deferred task
         * that operates on a GameInstance/Node from a non main thread then you need to do a few
         * additional checks inside of your deferred task, for example:
         * @code
         * // We are on a non-main thread inside of a node:
         * addDeferredTask([this, iNodeId](){ // capturing `this` to use `Node` (self) functions, also
         * capturing self ID
         *     // We are inside of a deferred task (on the main thread) and we don't know if the node (`this`)
         *     // was garbage collected or not because we submitted our task from a non-main thread.
         *     // REMEMBER: we can't capture `gc` pointers in `std::function`, this is not supported
         *     // and will cause memory leaks/crashes!
         *
         *     const auto pGameManager = GameManager::get(); // using engine's private class `GameManager`
         *
         *     // `pGameManager` is guaranteed to be valid inside of a deferred task.
         *     // Otherwise, if running this code outside of a deferred task you need to do 2 checks:
         *     // if (pGameManager == nullptr) return;
         *     // if (pGameManager->isBeingDestroyed()) return;
         *
         *      if (!pGameManager->isNodeSpawned(iNodeId)){
         *          // Node was despawned and it may be dangerous to run the callback.
         *          return;
         *       }
         *
         *       // Can safely interact with `this` (self) - we are on the main thread.
         *   });
         * @endcode
         *
         * @remark In the task you don't need to check if the game is being destroyed,
         * the engine makes sure all tasks are finished before the game is destroyed.
         *
         * @param task Function to execute.
         *
         */
        void addDeferredTask(const std::function<void()>& task) const;

        /**
         * Adds a function to be executed on the thread pool.
         *
         * @warning Don't capture `gc` pointers in `std::function`, this is not supported and
         * will cause memory leaks/crashes!
         *
         * @warning If you are using a member functions/fields inside of the task you need to make
         * sure that the owner object of these member functions/fields will not be deleted until
         * this task is finished.
         *
         * @remark In the task you don't need to check if the game is being destroyed,
         * the engine makes sure all tasks are finished before the game is destroyed.
         *
         * @param task Function to execute.
         */
        void addTaskToThreadPool(const std::function<void()>& task) const;

        /**
         * Adds a deferred task (see @ref addDeferredTask) to create a new world that contains only
         * one node - root node.
         *
         * @warning If you are holding any `gc` pointers to nodes in your game instance,
         * make sure you clear them (set to `nullptr`) before calling this function.
         *
         * @remark Replaces the old world (if existed).
         *
         * @remark Engine will execute all deferred tasks before changing the world (before destroying
         * all nodes), so even if deferred tasks queue looks like this:
         * ... -- create/load world task -- call node's member function task -- ...,
         * on `create/load world task` the engine will finish all other tasks and only when deferred
         * tasks queue is empty start to create/load world so you don't need to care about the order
         * of deferred tasks.
         *
         * @param onCreated  Callback function that will be called on the main thread after the world is
         * created. Contains optional error (if world creation failed) as the only argument. Use
         * GameInstance member functions as callback functions for created worlds, because all nodes
         * and other game objects will be destroyed while the world is changing.
         * @param iWorldSize Size of the new world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size.
         * You don't need to care why we need this information, you only need to know that if
         * you leave world bounds lighting or physics may be incorrect
         * (the editor or engine will warn you if something is leaving world bounds, pay attention
         * to the logs).
         */
        void createWorld(
            const std::function<void(const std::optional<Error>&)>& onCreated,
            size_t iWorldSize = 1024); // NOLINT: seems like a typical world size

        /**
         * Adds a deferred task (see @ref addDeferredTask)
         * to load and deserialize a node tree to be used as the new world.
         *
         * Node tree's root node will be used as world's root node.
         *
         * @warning If you are holding any `gc` pointers to nodes in game instance,
         * make sure you set `nullptr` to them before calling this function.
         *
         * @remark Replaces the old world (if existed).
         *
         * @remark Engine will execute all deferred tasks before changing the world (before destroying
         * all nodes), so even if deferred tasks queue looks like this:
         * ... -- create/load world task -- call node's member function task -- ...,
         * on `create/load world task` the engine will finish all other tasks and only when deferred
         * tasks queue is empty start to create/load world so you don't need to care about the order
         * of deferred tasks.
         *
         * @param onLoaded       Callback function that will be called on the main thread after the world is
         * loaded. Contains optional error (if world loading failed) as the only argument. Use
         * GameInstance member functions as callback functions for loaded worlds, because all nodes
         * and other game objects will be destroyed while the world is changing.
         * @param pathToNodeTree Path to the file that contains a node tree to load, the ".toml"
         * extension will be automatically added if not specified.
         * @param iWorldSize     Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size.
         * You don't need to care why we need this information, you only need to know that if
         * you leave world bounds lighting or physics may be incorrect
         * (the editor or engine will warn you if something is leaving world bounds, pay attention
         * to the logs).
         */
        void loadNodeTreeAsWorld(
            const std::function<void(const std::optional<Error>&)>& onLoaded,
            const std::filesystem::path& pathToNodeTree,
            size_t iWorldSize = 1024); // NOLINT: seems like a typical world size

        /**
         * Queues a request to run a garbage collection as a deferred task on the main thread
         * using @ref addDeferredTask.
         *
         * @remark Typically you don't need to call this function as garbage collection is executed
         * regularly (see @ref setGarbageCollectorRunInterval) but you can still call it anyway.
         *
         * @param bForce     Force run garbage collection even if the last garbage collection was run
         * not so long ago.
         * @param onFinished Optional callback that will be triggered on the main thread
         * when garbage collection is finished (queued as @ref addDeferredTask).
         */
        void queueGarbageCollection(bool bForce, const std::optional<std::function<void()>>& onFinished = {});

        /**
         * Modifies the interval after which we need to run garbage collector again.
         * The current value can be retrieved using @ref getGarbageCollectorRunIntervalInSec.
         *
         * @remark Interval should be in range [30; 300] seconds (otherwise it will be clamped).
         *
         * @remark Note that garbage collection will also be executed additionally in some special cases,
         * such as when World is being destructed.
         *
         * @param iGcRunIntervalInSec Interval in seconds.
         */
        void setGarbageCollectorRunInterval(long long iGcRunIntervalInSec);

        /**
         * Analyzes the current state to see if any shader-related errors have place (like unused
         * shaders in memory or etc.). Fixes errors and reports them in log.
         *
         * @remark Generally should be called right before you let the player play the game (after
         * all required nodes are spawned).
         */
        void executeShaderManagerSelfValidation() const;

        /**
         * Returns a pointer to world's root node.
         *
         * @return nullptr if world is not created or was destroyed (see @ref createWorld), otherwise world's
         * root node.
         */
        gc<Node> getWorldRootNode() const;

        /**
         * Returns time since world creation (in seconds).
         *
         * @return Zero if world is not created (see @ref createWorld), otherwise time since
         * world creation (in seconds).
         */
        float getWorldTimeInSeconds() const;

        /**
         * Returns world size in game units.
         *
         * @return World size.
         */
        size_t getWorldSize() const;

        /**
         * Returns total amount of currently spawned nodes.
         *
         * @return Total nodes spawned right now.
         */
        size_t getTotalSpawnedNodeCount();

        /**
         * Returns the current amount of spawned nodes that are marked as "should be called every frame".
         *
         * @return Amount of spawned nodes that should be called every frame.
         */
        size_t getCalledEveryFrameNodeCount();

        /**
         * Returns a reference to the window this game instance is using.
         *
         * @return A pointer to the window, should not be deleted.
         */
        Window* getWindow() const;

        /**
         * Returns a reference to the camera manager this game is using.
         *
         * @return A pointer to the camera manager, should not be deleted.
         */
        CameraManager* getCameraManager() const;

        /**
         * Returns a reference to the input manager this game instance is using.
         * Input manager allows binding IDs with multiple input keys that
         * you can receive in @ref onInputActionEvent.
         *
         * @return A pointer to the input manager, should not be deleted.
         */
        InputManager* getInputManager() const;

        /**
         * Returns the current interval after which we need to run garbage collector again.
         *
         * @return Interval in seconds.
         */
        long long getGarbageCollectorRunIntervalInSec();

    protected:
        /**
         * Creates a new timer and saves it inside of this GameInstance.
         *
         * @warning Do not free (delete) returned pointer.
         * @warning Do not use returned pointer outside of this object as the timer is only guaranteed
         * to live while the GameInstance (that created the timer) is living.
         *
         * @remark This function will not work and will log an error if you would try to create
         * a timer inside of the @ref onWindowClose function.
         * @remark This function exists to add some protection code to not shoot yourself in the foot,
         * such as: GameInstance will automatically stop and disable created timers
         * before @ref onWindowClose is called by using Timer::stop(true)
         * so that you don't have to remember to stop created timers. Moreover, if you are using
         * a callback function for the timer's timeout event it's guaranteed that this callback
         * function will only be called while the object is valid.
         * @remark There is no `removeTimer` function but it may appear in the future
         * (although there's really no point in removing a timer so don't care about it).
         *
         * @param sTimerName Name of this timer (used for logging). Don't add "timer" word to your timer's
         * name as it will be appended in the logs.
         *
         * @return `nullptr` if something went wrong, otherwise a non-owning pointer to the created timer
         * that is guaranteed to be valid while this object is alive.
         */
        Timer* createTimer(const std::string& sTimerName);

        /**
         * Called by owner object to stop and disable all created timers,
         * additionally does not allows creating any more timers.
         */
        void stopAndDisableCreatedTimers();

        /**
         * Called after GameInstance's constructor is finished and created
         * GameInstance object was saved in GameManager (that owns GameInstance).
         *
         * At this point you can create and interact with the game world and etc.
         */
        virtual void onGameStarted() {}

        /**
         * Called before a new frame is rendered.
         *
         * @remark Called before nodes that should be called every frame.
         *
         * @param timeSincePrevCallInSec Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float timeSincePrevCallInSec) {}

        /**
         * Called when the window receives keyboard input.
         * Called before @ref onInputActionEvent.
         * Prefer to use @ref onInputActionEvent instead of this function.
         *
         * @param key            Keyboard key.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        virtual void onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {}

        /**
         * Called when the window receives mouse input.
         * Called before @ref onInputActionEvent.
         * Prefer to use @ref onInputActionEvent instead of this function.
         *
         * @param button         Mouse button.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the button down event occurred or button up.
         */
        virtual void onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {}

        /**
         * Called when the window received mouse movement.
         *
         * @param xOffset  Mouse X movement delta in pixels (plus if moved to the right,
         * minus if moved to the left).
         * @param yOffset  Mouse Y movement delta in pixels (plus if moved up,
         * minus if moved down).
         */
        virtual void onMouseMove(double xOffset, double yOffset){};

        /**
         * Called when the window receives mouse scroll movement.
         *
         * @param iOffset Movement offset.
         */
        virtual void onMouseScrollMove(int iOffset) {}

        /**
         * Called when the window focus was changed.
         *
         * @param bIsFocused  Whether the window has gained or lost the focus.
         */
        virtual void onWindowFocusChanged(bool bIsFocused) {}

        /**
         * Called when the framebuffer size was changed.
         *
         * @param iWidth  New width of the framebuffer (in pixels).
         * @param iHeight New height of the framebuffer (in pixels).
         */
        virtual void onFramebufferSizeChanged(int iWidth, int iHeight) {}

        /**
         * Called when the window that owns this game instance
         * was requested to close (no new frames will be rendered).
         *
         * Prefer to have your destructor logic here, because after this function is finished
         * the world will be destroyed and will be inaccessible (`nullptr`).
         */
        virtual void onWindowClose() {}

        /**
         * Returns map of action events that this GameInstance is binded to (must be used with mutex).
         * Binded callbacks will be automatically called when an action event is triggered.
         *
         * @remark Note that nodes can also have their input event bindings and you may prefer
         * to bind to input in specific nodes instead of binding to them in GameInstance.
         * @remark Only events in GameInstance's InputManager (GameInstance::getInputManager)
         * will be considered to trigger events in the node.
         * @remark Called after @ref onKeyboardInput.
         *
         * Example:
         * @code
         * const unsigned int iForwardActionId = 0;
         * const auto pMtxActionEvents = getActionEventBindings();
         *
         * std::scoped_lock guard(pMtxActionEvents->first);
         * pMtxActionEvents->second[iForwardActionId] = [&](KeyboardModifiers modifiers, bool
         * bIsPressedDown) {
         *     moveForward(modifiers, bIsPressedDown);
         * };
         * @endcode
         *
         * @return Binded action events.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, bool)>>>*
        getActionEventBindings();

        /**
         * Returns map of axis events that this GameInstance is binded to (must be used with mutex).
         * Binded callbacks will be automatically called when an axis event is triggered.
         *
         * @remark Note that nodes can also have their input event bindings and you may prefer
         * to bind to input in specific nodes instead of binding to them in GameInstance.
         * @remark Only events in GameInstance's InputManager (GameInstance::getInputManager)
         * will be considered to trigger events in the node.
         * @remark Called after @ref onKeyboardInput.
         *
         * Example:
         * @code
         * const auto iForwardAxisEventId = 0;
         * const auto pMtxAxisEvents = getAxisEventBindings();
         *
         * std::scoped_lock guard(pMtxAxisEvents->first);
         * pMtxAxisEvents->second[iForwardAxisEventId] = [&](KeyboardModifiers modifiers, float input) {
         *     moveForward(modifiers, input);
         * };
         * @endcode
         *
         * @remark Input parameter is a value in range [-1.0f; 1.0f] that describes input.
         *
         * @return Binded action events.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, float)>>>*
        getAxisEventBindings();

    private:
        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an action event in the InputManager.
         * Called after @ref onKeyboardInput.
         *
         * @param iActionId      Unique ID of the input action event (from input manager).
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void onInputActionEvent(unsigned int iActionId, KeyboardModifiers modifiers, bool bIsPressedDown);

        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an axis event in the InputManager.
         * Called after @ref onKeyboardInput and after @ref onInputActionEvent.
         *
         * @param iAxisEventId  Unique ID of the input axis event (from input manager).
         * @param modifiers     Keyboard modifier keys.
         * @param input         A value in range [-1.0f; 1.0f] that describes input.
         */
        void onInputAxisEvent(unsigned int iAxisEventId, KeyboardModifiers modifiers, float input);

        /** Map of action events that this GameInstance is binded to. Must be used with mutex. */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, bool)>>>
            mtxBindedActionEvents;

        /** Map of axis events that this GameInstance is binded to. Must be used with mutex. */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, float)>>>
            mtxBindedAxisEvents;

        /**
         * Timers creates via @ref createTimer.
         *
         * @warning Don't remove/erase timers from this array because in callback validator's deferred task
         * we will use the timer to check its state so we need to make
         * sure that stopped timer will not be deleted while the object exists.
         */
        std::pair<std::recursive_mutex, std::vector<std::unique_ptr<Timer>>> mtxCreatedTimers;

        /** Whether @ref createTimer works or not. */
        bool bAllowCreatingTimers = true;

        /** Do not delete. Owner of @ref pGameManager object. */
        Window* pGameWindow = nullptr;

        /** Do not delete. Owner of this object. */
        GameManager* pGameManager = nullptr;

        /**
         * Do not delete. Input manager of the @ref pGameManager object.
         */
        InputManager* pInputManager = nullptr;
    };
} // namespace ne
