#include "Game.h"

// Custom.
#include "render/IRenderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

namespace ne {
    void Game::onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onKeyboardInput(key, modifiers, bIsPressedDown);

        // Find action by key.
        for (auto it = inputManager.actionEvents.begin(); it != inputManager.actionEvents.end(); ++it) {
            for (const auto &actionKey : it->second) {
                if (std::holds_alternative<KeyboardKey>(actionKey) &&
                    std::get<KeyboardKey>(actionKey) == key) {
                    pGameInstance->onInputActionEvent(it->first, key, modifiers, bIsPressedDown);
                }
            }
        }
    }

    void Game::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onMouseInput(button, modifiers, bIsPressedDown);

        // Find action by button.
        for (auto it = inputManager.actionEvents.begin(); it != inputManager.actionEvents.end(); ++it) {
            for (const auto &actionKey : it->second) {
                if (std::holds_alternative<MouseButton>(actionKey) &&
                    std::get<MouseButton>(actionKey) == button) {
                    pGameInstance->onInputActionEvent(it->first, button, modifiers, bIsPressedDown);
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
