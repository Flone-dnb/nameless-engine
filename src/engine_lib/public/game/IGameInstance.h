#pragma once

// Custom.
#include "input/KeyboardKey.hpp"

namespace ne {
    class Window;

    /**
     * Reacts to user inputs, window events and etc.
     */
    class IGameInstance {
    public:
        IGameInstance() = delete;
        explicit IGameInstance(Window *pGameWindow);

        IGameInstance(const IGameInstance &) = delete;
        IGameInstance &operator=(const IGameInstance &) = delete;

        virtual ~IGameInstance() = default;

        /**
         * Called before a frame is rendered.
         *
         * @param fTimeFromPrevCallInSec   Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec){};

        /**
         * Called when the window receives keyboard input.
         *
         * @param key        Keyboard key.
         * @param modifiers  Keyboard modifier keys.
         * @param action     Type of keyboard action.
         */
        virtual void onKeyInput(KeyboardKey key, KeyboardModifiers modifiers, KeyboardAction action){};

        /**
         * Called when the window focus was changed.
         *
         * @param bIsFocused  Whether the window has gained or lost the focus.
         */
        virtual void onWindowFocusChanged(bool bIsFocused){};

        /**
         * Called when the window was requested to close (no new frames will be rendered).
         */
        virtual void onWindowClose(){};

        /**
         * Returns the time in seconds that has passed
         * since the very first window was created.
         *
         * @return Time in seconds.
         */
        static float getTotalApplicationTimeInSec();

        /**
         * Returns a reference to the window this Game Instance is using.
         *
         * @return A pointer to the window, should not be deleted.
         */
        [[nodiscard]] inline Window *getGameWindow() const;

    private:
        /**
         * A reference to a window-owner of this Game Instance.
         * Should not be deleted.
         */
        Window *pGameWindow = nullptr;
    };

} // namespace ne