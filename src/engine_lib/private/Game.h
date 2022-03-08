#pragma once

// Std.
#include <memory>

#include "IGameInstance.h"

namespace ne {
    class IGameInstance;
    class IRenderer;
    class Window;

    /**
     * Holds main game objects: game instance, renderer,
     * audio engine, physics engine and etc.
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
        void setGameInstance(Window *pGameWindow) {
            pGameInstance = std::make_unique<GameInstance>(pGameWindow);
        }

    private:
        // The object should be created by a Window instance.
        friend class Window;

        Game();

        /**
         * Reacts to user inputs, window events and etc.
         */
        std::unique_ptr<IGameInstance> pGameInstance;

        /**
         * Draws the graphics on a window.
         */
        std::unique_ptr<IRenderer> pRenderer;
    };
} // namespace ne
