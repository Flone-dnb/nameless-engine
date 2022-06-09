#pragma once

// Std.
#include <memory>
#include <functional>
#include <queue>
#include <mutex>

// Custom.
#include "game/IGameInstance.h"
#include "input/InputManager.h"
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"
#include "misc/ThreadPool.h"

namespace ne {
    constexpr auto sGameLogCategory = "Game";

    class IGameInstance;
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
         * Set IGameInstance derived class to react to
         * user inputs, window events and etc.
         */
        template <typename GameInstance>
        requires std::derived_from<GameInstance, IGameInstance>
        void setGameInstance() { pGameInstance = std::make_unique<GameInstance>(pWindow, &inputManager); }

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
         * Returns window that owns this object.
         *
         * @return Do not delete this pointer. Window that owns this object.
         */
        Window* getWindow() const;

    private:
        // The object should be created by a Window instance.
        friend class Window;

        /**
         * Constructor.
         *
         * @param pWindow Window that owns this Game object.
         */
        Game(Window* pWindow);

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
         * Do not delete this pointer. Window-owner of this Game.
         */
        Window* pWindow;

        /**
         * Reacts to user input, window events and etc.
         */
        std::unique_ptr<IGameInstance> pGameInstance;

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
    };
} // namespace ne
