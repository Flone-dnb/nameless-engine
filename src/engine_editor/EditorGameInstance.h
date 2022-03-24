#pragma once

// Custom.
#include "game/IGameInstance.h"
#include "game/Window.h"

/**
 * Editor logic.
 */
class EditorGameInstance : public ne::IGameInstance {
public:
    EditorGameInstance(ne::Window *pGameWindow);
    virtual ~EditorGameInstance() override = default;

    /**
     * Called before a frame is rendered.
     *
     * @param fTimeFromPrevCallInSec   Time in seconds that has passed since the last call
     * to this function.
     */
    virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) override{};

    /**
     * Called when the window receives keyboard input.
     *
     * @param key            Keyboard key.
     * @param modifiers      Keyboard modifier keys.
     * @param bIsPressedDown Whether the key down event occurred or key up.
     */
    virtual void onKeyInput(ne::KeyboardKey key, ne::KeyboardModifiers modifiers,
                            bool bIsPressedDown) override;

    /**
     * Called when the window focus was changed.
     *
     * @param bIsFocused  Whether the window has gained or lost the focus.
     */
    virtual void onWindowFocusChanged(bool bIsFocused) override{};

    /**
     * Called when the window was requested to close (no new frames will be rendered).
     */
    virtual void onWindowClose() override{};
};
