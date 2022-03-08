#pragma once
#include <Windows.h>
// Custom.
#include "IGameInstance.h"
#include "Window.h"

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
     * @param key        Keyboard key.
     * @param modifiers  Keyboard modifier keys.
     * @param action     Type of keyboard action.
     */
    virtual void onKeyInput(ne::KeyboardKey key, ne::KeyboardModifiers modifiers,
                            ne::KeyboardAction action) override;

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
