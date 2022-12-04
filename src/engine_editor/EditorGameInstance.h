﻿#pragma once

// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"

/**
 * Editor logic.
 */
class EditorGameInstance : public ne::GameInstance {
public:
    /**
     * Constructor.
     *
     * @remark There is no need to save window/input manager pointers
     * in derived classes as the base class already saves these pointers and
     * provides @ref getWindow and @ref getInputManager functions.
     *
     * @param pWindow       Window that owns this game instance.
     * @param pInputManager Input manager of the owner Game object.
     */
    EditorGameInstance(ne::Window* pWindow, ne::InputManager* pInputManager);

    virtual ~EditorGameInstance() override = default;
};
