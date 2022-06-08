#pragma once

// STL.
#include <functional>

// Custom.
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"
#include "misc/Timer.h"

namespace ne {
    class Window;
    class InputManager;
    class Game;

    /**
     * Reacts to user inputs, window events and etc.
     * Owned by Game object.
     */
    class IGameInstance {
    public:
        IGameInstance() = delete;
        /**
         * Constructor.
         *
         * @warning There is no need to save window/input manager pointers
         * in derived classes as the base class already saves these pointers and
         * provides @ref getWindow and @ref getInputManager functions.
         *
         * @param pGameWindow   Window that owns this game instance.
         * @param pInputManager Input manager of the owner Game object.
         */
        explicit IGameInstance(Window* pGameWindow, InputManager* pInputManager);

        IGameInstance(const IGameInstance&) = delete;
        IGameInstance& operator=(const IGameInstance&) = delete;

        virtual ~IGameInstance() = default;

        /**
         * Returns the time in seconds that has passed
         * since the very first window was created.
         *
         * @return Time in seconds.
         */
        static float getTotalApplicationTimeInSec();

        /**
         * Called before a new frame is rendered.
         *
         * @param fTimeSincePrevCallInSec   Time in seconds that has passed since the last call
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
         * Creates a new timer.
         *
         * @return New timer.
         */
        std::shared_ptr<Timer> createTimer() const;

        /**
         * Adds a function to be executed on the main thread next time @ref onBeforeNewFrame
         * is called.
         *
         * @param task Function to execute.
         *
         * @warning If you are using member functions as tasks you need to make
         * sure that the owner object of these member functions will not be deleted until
         * this task is finished. IGameInstance member functions are safe to use.
         */
        void addDeferredTask(const std::function<void()>& task) const;

        /**
         * Adds a function to be executed on the thread pool.
         *
         * @param task Function to execute.
         *
         * @warning If you are using member functions as tasks you need to make
         * sure that the owner object of these member functions will not be deleted until
         * this task is finished. IGameInstance member functions are safe to use.
         */
        void addTaskToThreadPool(const std::function<void()>& task) const;

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