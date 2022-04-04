#include "EditorGameInstance.h"

EditorGameInstance::EditorGameInstance(ne::Window *pWindow, ne::InputManager *pInputManager)
    : IGameInstance(pWindow, pInputManager) {}

void EditorGameInstance::onInputActionEvent(const std::string &sActionName, ne::KeyboardModifiers modifiers,
                                            bool bIsPressedDown) {}

void EditorGameInstance::onMouseInput(ne::MouseButton button, ne::KeyboardModifiers modifiers,
                                      bool bIsPressedDown) {}

void EditorGameInstance::onKeyboardInput(ne::KeyboardKey key, ne::KeyboardModifiers modifiers,
                                         bool bIsPressedDown) {}
