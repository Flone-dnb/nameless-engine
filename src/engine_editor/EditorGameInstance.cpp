#include "EditorGameInstance.h"

// Custom.
#include "io/Logger.h"
#include "render/IRenderer.h"
#include "shaders/hlsl/HlslShader.h"

EditorGameInstance::EditorGameInstance(ne::Window* pWindow, ne::InputManager* pInputManager)
    : IGameInstance(pWindow, pInputManager) {
    const ne::ShaderDescription description(
        "test",
        "res/engine/shaders/default.hlsl",
        ne::ShaderType::VERTEX_SHADER,
        "VS",
        std::vector<std::string>());
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
    auto err = getWindow()->getRenderer()->getShaderManager()->compileShaders(
        std::vector{description}, onProgress, onError, [this]() { onShaderCompilationFinished(); });
    if (err.has_value()) {
        ne::Logger::get().error(err->getError(), "");
    }
}

void EditorGameInstance::onShaderCompilationFinished() {
    ne::Logger::get().info("received shader compilation finish", "");
    const auto shader = getWindow()->getRenderer()->getShaderManager()->getShader("test");
    if (!shader.has_value()) {
        ne::Logger::get().error("shader value was not set", "");
        return;
    }

    const auto pShader = dynamic_cast<ne::HlslShader*>(shader.value());
    const auto result = pShader->getCompiledBlob();
    if (std::holds_alternative<ne::Error>(result)) {
        auto err = std::get<ne::Error>(result);
        err.addEntry();
        err.showError();
        throw std::runtime_error(err.getError());
    }

    auto pBlob = std::get<Microsoft::WRL::ComPtr<IDxcBlob>>(result);
}

void EditorGameInstance::onInputActionEvent(
    const std::string& sActionName, ne::KeyboardModifiers modifiers, bool bIsPressedDown) {}

void EditorGameInstance::onInputAxisEvent(
    const std::string& sAxisName, ne::KeyboardModifiers modifiers, float fValue) {}

void EditorGameInstance::onMouseMove(int iXOffset, int iYOffset) {}

void EditorGameInstance::onMouseScrollMove(int iOffset) {}
