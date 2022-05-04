#include "shaders/ShaderManager.h"

// STL.
#include <format>
#include <chrono>
#include <ranges>

// Custom.
#include "hlsl/HlslShader.h"
#include "io/Logger.h"
#include "render/directx/DirectXRenderer.h"
#include "shaders/IShader.h"

namespace ne {
    ShaderManager::ShaderManager(IRenderer *pRenderer) { this->pRenderer = pRenderer; }

    ShaderManager::~ShaderManager() {
        bIsShuttingDown.test_and_set();

        std::scoped_lock<std::mutex> guard(mtxRwShaders);

        // Wait for shader compilation threads to finish early.
        if (!vRunningCompilationThreads.empty()) {
            Logger::get().info(
                std::format(
                    "waiting for {} shader compilation thread(-s) to finish early",
                    vRunningCompilationThreads.size()),
                sShaderManagerLogCategory);
            for (auto &promise : vRunningCompilationThreads) {
                promise.get_future().get();
            }
        }
    }

    std::optional<Error> ShaderManager::compileShaders(
        const std::vector<ShaderDescription> &vShadersToCompile,
        std::function<void(size_t, size_t)> onProgress,
        std::function<void()> onCompleted) {
        if (vShadersToCompile.empty()) {
            return Error("the specified array of shaders to compile is empty");
        }

        // Check if shutting down.
        if (bIsShuttingDown.test(std::memory_order_seq_cst)) {
            return {};
        }

        std::scoped_lock<std::mutex> guard(mtxRwShaders);

        // Check if we already have a shader with this name.
        for (const auto &shader : vShadersToCompile) {
            auto it = shaders.find(shader.sShaderName);
            if (it != shaders.end()) {
                return Error(std::format(
                    "A shader with the name \"{}\" was already added, "
                    "please choose another name for this shader.",
                    shader.sShaderName));
            }
        }

        // Remove finished promises.
        const auto [first, last] = std::ranges::remove_if(vRunningCompilationThreads, [](auto &promise) {
            using namespace std::chrono_literals;
            const auto status = promise.get_future().wait_for(1ms);
            return status == std::future_status::ready;
        });
        vRunningCompilationThreads.erase(first, last);

        { // TODO: in other thread:
            for (const auto &shaderDescription : vShadersToCompile) {
                if (dynamic_cast<DirectXRenderer *>(pRenderer)) {
                    IShader::compileShader<HlslShader>(shaderDescription);
                } else {
                    const Error err("not implemented shader type");
                    err.showError();
                    throw std::runtime_error(err.getError());
                }
            }
        }
        // TODO: do all work in another thread
        // TODO: use mutex to sync access to shaders map.

        // TODO: on compilation thread:
        // TODO: - use "-D" compilation flag to add macro
        // TODO: - use mtxRwShaders to add new shaders
        // TODO: - check bIsShuttingDown each shader and abort if needed (+ set promise + write to log)
        // TODO: - call onProgress (+ write to log with shader name)
        // TODO: - call onCompleted and onProgress (+ write to log with shader name)
        // TODO: - set promise on exit

        return {};
    }
} // namespace ne
