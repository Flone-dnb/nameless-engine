#include "EditorGameInstance.h"

// Custom.
#include "io/Logger.h"
#include "render/IRenderer.h"

EditorGameInstance::EditorGameInstance(ne::Window* pWindow, ne::InputManager* pInputManager)
    : IGameInstance(pWindow, pInputManager) {
    const ne::ShaderDescription description(
        "test",
        "res/engine/shaders/default.hlsl",
        ne::ShaderType::VERTEX_SHADER,
        L"VS",
        std::vector<std::wstring>());
    size_t iThreadId = std::hash<std::thread::id>()(std::this_thread::get_id());
    auto onProgress = [iThreadId](size_t iCompiledShaderCount, size_t iTotalShadersToCompile) {
        ne::Logger::get().info(
            std::format(
                "received progress {}/{} on thread {}",
                iCompiledShaderCount,
                iTotalShadersToCompile,
                iThreadId),
            "");
    };
    auto onError =
        [iThreadId](ne::ShaderDescription shaderDescription, std::variant<std::string, ne::Error> error) {
            ne::Logger::get().info(std::format("received error on thread {}", iThreadId), "");
            if (std::holds_alternative<std::string>(error)) {
                ne::Logger::get().info(std::get<std::string>(error), "");
            } else {
                ne::Logger::get().info(std::get<ne::Error>(error).getError(), "");
            }
        };
    auto onCompleted = [iThreadId]() {
        ne::Logger::get().info(std::format("received completed on thread {}", iThreadId), "");
    };
    getWindow()->getRenderer()->getShaderManager()->compileShaders(
        std::vector{description}, onProgress, onError, onCompleted);
}

void EditorGameInstance::onInputActionEvent(
    const std::string& sActionName, ne::KeyboardModifiers modifiers, bool bIsPressedDown) {}

void EditorGameInstance::onInputAxisEvent(
    const std::string& sAxisName, ne::KeyboardModifiers modifiers, float fValue) {}

void EditorGameInstance::onMouseMove(int iXOffset, int iYOffset) {}

void EditorGameInstance::onMouseScrollMove(int iOffset) {}
