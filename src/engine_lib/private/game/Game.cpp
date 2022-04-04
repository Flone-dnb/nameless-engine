#include "Game.h"

// Custom.
#include "render/IRenderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

namespace ne {
    void Game::onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onKeyboardInput(key, modifiers, bIsPressedDown);

        std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxActions);
        if (!inputManager.actionEvents.empty()) {
            const auto it = inputManager.actionEvents.find(key);
            if (it != inputManager.actionEvents.end()) {
                // TODO: find a better approach:
                // copying set of actions because it can be modified in onInputActionEvent by user code
                // while we are iterating over it (the user should be able to modify it).
                // This should not be that bad due to the fact that a key will usually have
                // only one action associated with it.
                // + update InputManager documentation.
                const auto actionsCopy = it->second;
                for (const auto &sActionName : actionsCopy) {
                    pGameInstance->onInputActionEvent(sActionName, modifiers, bIsPressedDown);
                }
            }
        }
    }

    void Game::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onMouseInput(button, modifiers, bIsPressedDown);

        std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxActions);
        if (!inputManager.actionEvents.empty()) {
            const auto it = inputManager.actionEvents.find(button);
            if (it != inputManager.actionEvents.end()) {
                // TODO: find a better approach:
                // copying set of actions because it can be modified in onInputActionEvent by user code
                // while we are iterating over it (the user should be able to modify it).
                // This should not be that bad due to the fact that a key will usually have
                // only one action associated with it.
                // + update InputManager documentation.
                const auto actionsCopy = it->second;
                for (const auto &sActionName : actionsCopy) {
                    pGameInstance->onInputActionEvent(sActionName, modifiers, bIsPressedDown);
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
