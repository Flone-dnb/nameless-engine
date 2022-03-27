#pragma once

// STL.
#include <variant>

// Custom.
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"

namespace ne {
    class Window;
    class InputManager;

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
         * provides @ref getGameWindow and @ref getInputManager functions.
         *
         * @param pGameWindow   Window that owns this game instance.
         * @param pInputManager Input manager of the owner Game object.
         */
        explicit IGameInstance(Window *pGameWindow, InputManager *pInputManager);

        IGameInstance(const IGameInstance &) = delete;
        IGameInstance &operator=(const IGameInstance &) = delete;

        virtual ~IGameInstance() = default;

        /**
         * Called before a new frame is rendered.
         *
         * @param fTimeFromPrevCallInSec   Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) {}

        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an action in the input manager.
         * Called after @ref onKeyInput.
         *
         * @param sActionName    Name of the input action (from input manager).
         * @param key            Keyboard/mouse key/button.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        virtual void onInputAction(const std::string &sActionName, std::variant<KeyboardKey, MouseButton> key,
                                   KeyboardModifiers modifiers, bool bIsPressedDown) {}

        /**
         * Called when the window receives keyboard input.
         * Called before @ref onInputAction.
         * Prefer to use @ref onInputAction instead of this function.
         *
         * @param key            Keyboard key.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        virtual void onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {}

        /**
         * Called when the window receives mouse input.
         * Called before @ref onInputAction.
         * Prefer to use @ref onInputAction instead of this function.
         *
         * @param button         Mouse button.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the button down event occurred or button up.
         */
        virtual void onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {}

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
         * Returns the time in seconds that has passed
         * since the very first window was created.
         *
         * @return Time in seconds.
         */
        static float getTotalApplicationTimeInSec();

        /**
         * Returns a reference to the window this game instance is using.
         *
         * @return A pointer to the window, should not be deleted.
         */
        Window *getWindow() const;

        /**
         * Returns a reference to the input manager this game instance is using.
         * Input manager allows binding names with multiple input keys that
         * you can receive in @ref onInputAction.
         *
         * @return A pointer to the input manager, should not be deleted.
         */
        InputManager *getInputManager() const;

    private:
        /**
         * A reference to a window-owner of this Game Instance.
         * Should not be deleted.
         */
        Window *pGameWindow = nullptr;

        /**
         * A reference to a input manager of the owner Game object.
         * Should not be deleted.
         */
        InputManager *pInputManager = nullptr;
    };

} // namespace ne