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
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an action event in the InputManager.
         * Called after @ref onKeyboardInput.
         *
         * @param sActionName    Name of the input action event (from input manager).
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        virtual void
        onInputActionEvent(const std::string& sActionName, KeyboardModifiers modifiers, bool bIsPressedDown) {
        }

        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an axis event in the InputManager.
         * Called after @ref onKeyboardInput and after @ref onInputActionEvent.
         *
         * @param sAxisName      Name of the input axis event (from input manager).
         * @param modifiers      Keyboard modifier keys.
         * @param fValue         A value in range [-1.0f; 1.0f] that describes input.
         */
        virtual void
        onInputAxisEvent(const std::string& sAxisName, KeyboardModifiers modifiers, float fValue) {}

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
         * Called when a window that owns this game instance
         * was requested to close (no new frames will be rendered).
         * Prefer to do your destructor logic here.
         */
        virtual void onWindowClose() {}

        /**
         * Adds a function to be executed on the main thread next time @ref onBeforeNewFrame
         * is called.
         *
         * @warning If you are using member functions as tasks you need to make
         * sure that the owner object of these member functions will not be deleted until
         * this task is finished. IGameInstance member functions are safe to use.
         *
         * @param task Function to execute.
         *
         */
        void addDeferredTask(const std::function<void()>& task) const;

        /**
         * Adds a function to be executed on the thread pool.
         *
         * @warning If you are using member functions as tasks you need to make
         * sure that the owner object of these member functions will not be deleted until
         * this task is finished. IGameInstance member functions are safe to use.
         *
         * @param task Function to execute.
         */
        void addTaskToThreadPool(const std::function<void()>& task) const;

        /**
         * Creates a new world that contains only one node - root node.
         * Replaces the old world (if existed).
         *
         * @warning This function should be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         * Use @ref addDeferredTask if you are not sure.
         *
         * @warning If you are holding any `gc` pointers to nodes in game instance,
         * make sure you set `nullptr` to them before calling this function.
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
         * @warning If you are holding any `gc` pointers to nodes in game instance,
         * make sure you set `nullptr` to them before calling this function.
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
         * Queues a request to run a garbage collection as a deferred task on the main thread
         * using @ref addDeferredTask.
         *
         * @remark Typically you don't need to call this function as garbage collection is executed
         * regularly (see @ref setGarbageCollectorRunInterval) but you can still call it anyway.
         *
         * @param onFinished Optional callback that will be triggered on the main thread
         * when garbage collection is finished.
         */
        void queueGarbageCollection(std::optional<std::function<void()>> onFinished);

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

    private:
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
