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
     * provides @ref getWindow and @ref getInputManager functions.
     *
     * @param pWindow       Window that owns this game instance.
     * @param pInputManager Input manager of the owner Game object.
     */
    EditorGameInstance(ne::Window *pWindow, ne::InputManager *pInputManager);
    virtual ~EditorGameInstance() override = default;

    /**
     * Called before a frame is rendered.
     *
     * @param fTimeFromPrevCallInSec   Time in seconds that has passed since the last call
     * to this function.
     */
    virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) override{};

    /**
     * Called when a window that owns this game instance receives user
     * input and the input key exists as an action event in the input manager.
     * Called after @ref onKeyboardInput.
     *
     * @param sActionName    Name of the input action event (from input manager).
     * @param modifiers      Keyboard modifier keys.
     * @param bIsPressedDown Whether the key down event occurred or key up.
     */
    virtual void onInputActionEvent(const std::string &sActionName, ne::KeyboardModifiers modifiers,
                                    bool bIsPressedDown) override;

    /**
     * Called when the window receives mouse input.
     * Called before @ref onInputActionEvent.
     * Prefer to use @ref onInputActionEvent instead of this function.
     *
     * @param button         Mouse button.
     * @param modifiers      Keyboard modifier keys.
     * @param bIsPressedDown Whether the button down event occurred or button up.
     */
    virtual void onMouseInput(ne::MouseButton button, ne::KeyboardModifiers modifiers,
                              bool bIsPressedDown) override;

    /**
     * Called when the window receives keyboard input.
     * Called before @ref onInputActionEvent.
     * Prefer to use @ref onInputActionEvent instead of this function.
     *
     * @param key            Keyboard key.
     * @param modifiers      Keyboard modifier keys.
     * @param bIsPressedDown Whether the key down event occurred or key up.
     */
    virtual void onKeyboardInput(ne::KeyboardKey key, ne::KeyboardModifiers modifiers,
                                 bool bIsPressedDown) override;

    /**
     * Called when the window was requested to close (no new frames will be rendered).
     */
    virtual void onWindowClose() override {}
};
