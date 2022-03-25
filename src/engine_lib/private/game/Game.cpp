#include "Game.h"

// Custom.
#include "render/IRenderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

namespace ne {
    void Game::onKeyInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onKeyInput(key, modifiers, bIsPressedDown);

        // Find action by key.
        for (auto it = inputManager.actions.begin(); it != inputManager.actions.end(); ++it) {
            for (const auto &actionKey : it->second) {
                if (actionKey == key) {
                    pGameInstance->onInputAction(it->first, key, modifiers, bIsPressedDown);
                }
            }
        }
    }

    Game::Game(Window *pWindow) {
        this->pWindow = pWindow;

#if defined(WIN32)
        pRenderer = std::make_unique<DirectXRenderer>();
#elif __linux__
        static_assert(false, "need to assign renderer here");
#endif
    }

} // namespace ne
