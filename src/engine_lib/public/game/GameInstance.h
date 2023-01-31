#pragma once

// Standard.
#include <functional>

// Custom.
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"
#include "game/nodes/Node.h"

namespace ne {
    class Window;
    class InputManager;
    class Game;

    /**
     * Main game class, exists while the game window is not closed
     * (i.e. while the game is not closed).
     *
     * Reacts to user inputs, window events and etc.
     * Owned by Game object.
     */
    class GameInstance {
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
         * @param pInputManager Input manager of the owner Game object.
         */
        explicit GameInstance(Window* pGameWindow, InputManager* pInputManager);

        GameInstance(const GameInstance&) = delete;
        GameInstance& operator=(const GameInstance&) = delete;

        virtual ~GameInstance() = default;

        /**
         * Returns the time in seconds that has passed
         * since the very first window was created.
         *
         * @return Time in seconds.
         */
        static float getTotalApplicationTimeInSec();

        /**
         * Called after GameInstance's constructor is finished and created
         * GameInstance object was saved in Game object (that owns GameInstance).
         *
         * At this point you can create and interact with the game world and etc.
         */
        virtual void onGameStarted() {}

        /**
         * Called before a new frame is rendered.
         *
         * @param fTimeSincePrevCallInSec Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) {}

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
         * @param iXOffset  Mouse X movement delta in pixels (plus if moved to the right,
         * minus if moved to the left).
         * @param iYOffset  Mouse Y movement delta in pixels (plus if moved up,
         * minus if moved down).
         */
        virtual void onMouseMove(int iXOffset, int iYOffset) {}

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
         * Called when the window that owns this game instance
         * was requested to close (no new frames will be rendered).
         *
         * Prefer to have your destructor logic here, because after this function is finished
         * the world will be destroyed and will be inaccessible (`nullptr`).
         */
        virtual void onWindowClose() {}

        /**
         * Adds a function to be executed on the main thread next time @ref onBeforeNewFrame
         * is called.
         *
         * @warning If you are using a member function as a task you need to make
         * sure that the owner object of this member function will not be deleted until
         * this task is finished. GameInstance and Node member functions are safe to use.
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
         * @warning If you are using a member function as a task you need to make
         * sure that the owner object of this member function will not be deleted until
         * this task is finished. GameInstance member functions are safe to use.
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
            const std::function<void(const std::optional<Error>&)>& onCreated, size_t iWorldSize = 1024);

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
            size_t iWorldSize = 1024);

        /**
         * Queues a request to run a garbage collection as a deferred task on the main thread
         * using @ref addDeferredTask.
         *
         * @remark Typically you don't need to call this function as garbage collection is executed
         * regularly (see @ref setGarbageCollectorRunInterval) but you can still call it anyway.
         *
         * @param onFinished Optional callback that will be triggered on the main thread
         * when garbage collection is finished (queued as @ref addDeferredTask).
         */
        void queueGarbageCollection(const std::optional<std::function<void()>>& onFinished);

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
         * Returns a reference to the input manager this game instance is using.
         * Input manager allows binding names with multiple input keys that
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
         * const auto sForwardActionName = "forward";
         * const auto pMtxActionEvents = getActionEventBindings();
         *
         * std::scoped_lock guard(pMtxActionEvents->first);
         * pMtxActionEvents->second[sForwardActionName] = [&](KeyboardModifiers modifiers, bool
         * bIsPressedDown) {
         *     moveForward(modifiers, bIsPressedDown);
         * };
         * @endcode
         *
         * @return Binded action events.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<std::string, std::function<void(KeyboardModifiers, bool)>>>*
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
         * const auto sForwardAxisName = "forward";
         * const auto pMtxAxisEvents = getAxisEventBindings();
         *
         * std::scoped_lock guard(pMtxAxisEvents->first);
         * pMtxAxisEvents->second[sForwardAxisName] = [&](KeyboardModifiers modifiers, float input) {
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
            std::unordered_map<std::string, std::function<void(KeyboardModifiers, float)>>>*
        getAxisEventBindings();

    private:
        // Game will trigger input events.
        friend class Game;

        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an action event in the InputManager.
         * Called after @ref onKeyboardInput.
         *
         * @param sActionName    Name of the input action event (from input manager).
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void
        onInputActionEvent(const std::string& sActionName, KeyboardModifiers modifiers, bool bIsPressedDown);

        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an axis event in the InputManager.
         * Called after @ref onKeyboardInput and after @ref onInputActionEvent.
         *
         * @param sAxisName      Name of the input axis event (from input manager).
         * @param modifiers      Keyboard modifier keys.
         * @param input          A value in range [-1.0f; 1.0f] that describes input.
         */
        void onInputAxisEvent(const std::string& sAxisName, KeyboardModifiers modifiers, float input);

        /** Map of action events that this GameInstance  is binded to. Must be used with mutex. */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<std::string, std::function<void(KeyboardModifiers, bool)>>>
            mtxBindedActionEvents;

        /** Map of axis events that this GameInstance is binded to. Must be used with mutex. */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<std::string, std::function<void(KeyboardModifiers, float)>>>
            mtxBindedAxisEvents;

        /** Do not delete. Owner of @ref pGame object. */
        Window* pGameWindow = nullptr;

        /** Do not delete. Owner of this object. */
        Game* pGame = nullptr;

        /**
         * Do not delete. Input manager of the @ref pGame object.
         */
        InputManager* pInputManager = nullptr;
    };
} // namespace ne
