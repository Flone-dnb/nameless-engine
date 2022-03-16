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
        void setGameInstance() { pGameInstance = std::make_unique<GameInstance>(pWindow); }

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
         * Window that owns this object, thus raw pointer - do not delete.
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
    };
} // namespace ne
