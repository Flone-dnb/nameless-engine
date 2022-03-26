#pragma once

// Custom.
#include "game/IGameInstance.h"
#include "game/Window.h"

/**
 * Editor logic.
 */
class EditorGameInstance : public ne::IGameInstance {
public:
    /**
     * Constructor.
     *
     * @warning There is no need to save window/input manager pointers
     * in derived classes as the base class already saves these pointers and
     * provides @ref getGameWindow and @ref getInputManager functions.
     *
     * @param pWindow       Window that owns this game instance.
     * @param pInputManager Input manager of the owner Game object.
     */
    EditorGameInstance(ne::Window *pWindow, ne::InputManager *pInputManager)
        : IGameInstance(pWindow, pInputManager){};
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
    virtual void onKeyboardInput(ne::KeyboardKey key, ne::KeyboardModifiers modifiers,
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
