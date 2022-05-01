#pragma once

// Std.
#include <memory>

// Custom.
#include "game/IGameInstance.h"
#include "input/InputManager.h"
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"

namespace ne {
    class IGameInstance;
    class IRenderer;
    class Window;
    class ShaderManager;

    /**
     * Holds main game objects: game instance, input manager, renderer,
     * audio engine, physics engine and etc.
     * Owned by Window object.
     */
    class Game {
    public:
        Game(const Game &) = delete;
        Game &operator=(const Game &) = delete;

        virtual ~Game() = default;

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
         * @param fTimeFromPrevCallInSec   Time in seconds that has passed since the last call
         * to this function.
         */
        void onBeforeNewFrame(float fTimeFromPrevCallInSec) const;

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

    private:
        // The object should be created by a Window instance.
        friend class Window;

        /**
         * Constructor.
         *
         * @param pWindow Window that owns this Game object.
         */
        Game(Window *pWindow);

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
         * A reference to a window-owner of this Game.
         * Should not be deleted.
         */
        Window *pWindow;

        /**
         * Reacts to user inputs, window events and etc.
         */
        std::unique_ptr<IGameInstance> pGameInstance;

        /**
         * Draws the graphics on a window.
         */
        std::unique_ptr<IRenderer> pRenderer;

        /**
         * Controls shader compilation and shader registry.
         */
        std::unique_ptr<ShaderManager> pShaderManager;

        /** Binds action names with input. */
        InputManager inputManager;
    };
} // namespace ne
