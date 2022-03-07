#pragma once

// Std.
#include <memory>

namespace dxe {
    class IGameInstance;

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
        void setGameInstance() { pGameInstance = std::make_unique<GameInstance>(); }

    private:
        // The object should be created by a Window instance.
        friend class Window;

        Game() = default;

        /**
         * Reacts to user inputs, window events and etc.
         */
        std::unique_ptr<IGameInstance> pGameInstance;
    };
} // namespace dxe
