#pragma once

// Custom.
#include "game/GameInstance.h"

namespace ne {
    class Window;
    class Game;
    class InputManager;

    /** Defines editor game. */
    class EditorGameInstance : public GameInstance {
    public:
        /**
         * Constructor.
         *
         * @remark There is no need to save window/input manager pointers
         * in derived classes as the base class already saves these pointers and
         * provides @ref getWindow and @ref getInputManager functions.
         *
         * @param pWindow       Window that owns this game instance.
         * @param pGame         Game that owns this game instance.
         * @param pInputManager Input manager of the owner Game object.
         */
        EditorGameInstance(Window* pWindow, Game* pGame, InputManager* pInputManager);

        virtual ~EditorGameInstance() override = default;
    };
} // namespace ne
