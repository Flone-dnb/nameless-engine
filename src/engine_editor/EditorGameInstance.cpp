#include "EditorGameInstance.h"

#include "io/Logger.h"

EditorGameInstance::EditorGameInstance(ne::Window* pWindow, ne::InputManager* pInputManager)
    : IGameInstance(pWindow, pInputManager) {
    pTimer = createTimer();
    pTimer->setCallbackForTimeout([]() { ne::Logger::get().info("Hello World from timer timeout!", ""); });
    pTimer->start(5000);
}

void EditorGameInstance::onInputActionEvent(
    const std::string& sActionName, ne::KeyboardModifiers modifiers, bool bIsPressedDown) {}

void EditorGameInstance::onInputAxisEvent(
    const std::string& sAxisName, ne::KeyboardModifiers modifiers, float fValue) {}

void EditorGameInstance::onMouseMove(int iXOffset, int iYOffset) {}

void EditorGameInstance::onMouseScrollMove(int iOffset) {}

void EditorGameInstance::onWindowClose() {
    if (pTimer) {
        pTimer->stop();
    }
}
