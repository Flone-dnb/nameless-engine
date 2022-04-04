#include "Game.h"

// Custom.
#include "io/Logger.h"
#include "render/IRenderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

namespace ne {
    void Game::onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onKeyboardInput(key, modifiers, bIsPressedDown);

        // Trigger action events.
        {
            std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxActionEvents);
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

        // Trigger axis events.
        {
            std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxAxisEvents);
            if (!inputManager.axisEvents.empty()) {
                const auto it = inputManager.axisEvents.find(key);
                if (it != inputManager.axisEvents.end()) {
                    // TODO: find a better approach:
                    // copying set of axis because it can be modified in onInputAxisEvent by user code
                    // while we are iterating over it (the user should be able to modify it).
                    // This should not be that bad due to the fact that a key will usually have
                    // only one axis associated with it.
                    // + update InputManager documentation.
                    const auto axisCopy = it->second;
                    for (const auto &[sActionName, iInput] : axisCopy) {
                        auto stateIt = inputManager.axisState.find(sActionName);
                        if (stateIt == inputManager.axisState.end()) {
                            Logger::get().error(std::format("input manager returned 0 "
                                                            "states for '{}' axis event",
                                                            sActionName));
                            pGameInstance->onInputAxisEvent(
                                sActionName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0f);
                            continue;
                        }

                        // Mark current state.
                        bool bSet = false;
                        std::pair<std::vector<AxisState>, int /* last input */> &statePair = stateIt->second;
                        for (auto &state : statePair.first) {
                            if (iInput == 1 && state.plusKey == key) {
                                // Plus key.
                                state.isPlusKeyPressed = bIsPressedDown;
                                bSet = true;
                                break;
                            } else if (iInput == -1 && state.minusKey == key) {
                                // Minus key.
                                state.isMinusKeyPressed = bIsPressedDown;
                                bSet = true;
                                break;
                            }
                        }
                        if (bSet == false) {
                            Logger::get().error(std::format("could not find key '{}' in key "
                                                            "states for '{}' axis event",
                                                            getKeyName(key), sActionName));
                            pGameInstance->onInputAxisEvent(
                                sActionName, modifiers, bIsPressedDown ? static_cast<float>(iInput) : 0.0f);
                            continue;
                        }

                        int iInputToPass = bIsPressedDown ? iInput : 0;

                        if (bIsPressedDown == false) {
                            // See if we need to pass 0 input value.
                            // See if other button are pressed.
                            for (const AxisState &state : statePair.first) {
                                if (iInput == -1) {
                                    // Look for plus button.
                                    if (state.isPlusKeyPressed) {
                                        iInputToPass = 1;
                                        break;
                                    }
                                } else {
                                    // Look for minus button.
                                    if (state.isMinusKeyPressed) {
                                        iInputToPass = -1;
                                        break;
                                    }
                                }
                            }
                        }

                        if (iInputToPass != statePair.second) {
                            statePair.second = iInputToPass;
                            pGameInstance->onInputAxisEvent(sActionName, modifiers,
                                                            static_cast<float>(iInputToPass));
                        }
                    }
                }
            }
        }
    }

    void Game::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) {
        pGameInstance->onMouseInput(button, modifiers, bIsPressedDown);

        std::scoped_lock<std::recursive_mutex> guard(inputManager.mtxActionEvents);
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
