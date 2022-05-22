#include "shaders/ShaderManager.h"

// STL.
#include <format>
#include <chrono>
#include <ranges>
#include <thread>

// Custom.
#include "game/Game.h"
#include "io/Logger.h"
#include "shaders/IShader.h"
#include "render/IRenderer.h"
#include "io/ConfigManager.h"

namespace ne {
    ShaderManager::ShaderManager(IRenderer* pRenderer) { this->pRenderer = pRenderer; }

    ShaderManager::~ShaderManager() {
        bIsShuttingDown.test_and_set();

        std::scoped_lock<std::mutex> guard(mtxRwShaders);

        // Remove finished promises of compilation threads.
        const auto [first, last] = std::ranges::remove_if(vRunningCompilationThreads, [](auto& promise) {
            using namespace std::chrono_literals;
            try {
                const auto status = promise.get_future().wait_for(1ms);
                return status == std::future_status::ready;
            } catch (const std::exception& ex) {
                Logger::get().error(
                    std::format(
                        "one of the shader compilation threads returned "
                        "an exception: {}",
                        ex.what()),
                    sShaderManagerLogCategory);
                return true;
            }
        });
        vRunningCompilationThreads.erase(first, last);

        // Wait for shader compilation threads to finish early.
        if (!vRunningCompilationThreads.empty()) {
            Logger::get().info(
                std::format(
                    "waiting for {} shader compilation thread(-s) to finish early",
                    vRunningCompilationThreads.size()),
                sShaderManagerLogCategory);
            for (auto& promise : vRunningCompilationThreads) {
                try {
                    promise.get_future().get();
                } catch (const std::exception& ex) {
                    Logger::get().error(
                        std::format(
                            "one of the shader compilation threads returned "
                            "an exception: {}",
                            ex.what()),
                        sShaderManagerLogCategory);
                }
            }
            Logger::get().info(
                std::format("finished {} shader compilation thread(-s)", vRunningCompilationThreads.size()),
                sShaderManagerLogCategory);
        }
    }

    std::optional<IShader*> ShaderManager::getShader(const std::string& sShaderName) {
        std::scoped_lock<std::mutex> guard(mtxRwShaders);
        const auto it = shaders.find(sShaderName);
        if (it == shaders.end()) {
            return {};
        } else {
            return it->second.get();
        }
    }

    std::optional<Error> ShaderManager::compileShaders(
        const std::vector<ShaderDescription>& vShadersToCompile,
        const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
        const std::function<
            void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
        const std::function<void()>& onCompleted) {
        if (vShadersToCompile.empty()) [[unlikely]] {
            return Error("the specified array of shaders to compile is empty");
        }

        // Check if shutting down.
        if (bIsShuttingDown.test(std::memory_order_seq_cst)) {
            return {};
        }

        std::scoped_lock<std::mutex> guard(mtxRwShaders);

        // Check if we already have a shader with this name.
        for (const auto& shader : vShadersToCompile) {
            if (shader.sShaderName == sGlobalShaderCacheParametersFileName) [[unlikely]] {
                return Error(std::format(
                    "shader name \"{}\" could not be used as "
                    "it's reserved for internal purposes",
                    sGlobalShaderCacheParametersFileName));
            }

            auto it = shaders.find(shader.sShaderName);
            if (it != shaders.end()) [[unlikely]] {
                return Error(std::format(
                    "a shader with the name \"{}\" was already added, "
                    "please choose another name for this shader",
                    shader.sShaderName));
            }
        }

        // Remove finished promises of compilation threads.
        const auto [first, last] = std::ranges::remove_if(vRunningCompilationThreads, [](auto& promise) {
            using namespace std::chrono_literals;
            try {
                const auto status = promise.get_future().wait_for(1ms);
                return status == std::future_status::ready;
            } catch (const std::exception& ex) {
                Logger::get().error(
                    std::format(
                        "one of the shader compilation threads returned "
                        "an exception: {}",
                        ex.what()),
                    sShaderManagerLogCategory);
                return true;
            }
        });
        vRunningCompilationThreads.erase(first, last);

        // If build mode was changed clear shader cache directory.
#if defined(DEBUG)
        bool bIsReleaseBuild = false;
#else
        bool bIsReleaseBuild = true;
#endif
        ConfigManager configManager;

        const auto shaderCacheDir = IShader::getPathToShaderCacheDirectory();
        const auto shaderParamsPath =
            IShader::getPathToShaderCacheDirectory() / sGlobalShaderCacheParametersFileName;
        if (std::filesystem::exists(shaderParamsPath)) {
            auto result = configManager.loadFile(shaderParamsPath);
            if (result.has_value()) {
                result->addEntry();
                return result.value();
            }

            auto bOldShaderCacheInRelease = configManager.getBoolValue(
                sGlobalShaderCacheParametersSectionName,
                sGlobalShaderCacheParametersReleaseBuildKeyName,
                !bIsReleaseBuild);

            if (bOldShaderCacheInRelease != bIsReleaseBuild) {
                Logger::get().info(
                    "clearing shader cache directory because build mode was changed",
                    sShaderManagerLogCategory);
                std::filesystem::remove_all(shaderCacheDir);
                std::filesystem::create_directory(shaderCacheDir);

                configManager.setBoolValue(
                    sGlobalShaderCacheParametersSectionName,
                    sGlobalShaderCacheParametersReleaseBuildKeyName,
                    bIsReleaseBuild);

                result = configManager.saveFile(shaderParamsPath, false);
                if (result.has_value()) {
                    result->addEntry();
                    return result.value();
                }
            }
        } else {
            if (std::filesystem::exists(shaderCacheDir)) {
                std::filesystem::remove_all(shaderCacheDir);
                std::filesystem::create_directory(shaderCacheDir);
            }

            Logger::get().info(
                std::format(
                    "global shader cache parameters file was not found, creating a new {} configuration",
                    bIsReleaseBuild ? "release" : "debug"),
                sShaderManagerLogCategory);
            configManager.setBoolValue(
                sGlobalShaderCacheParametersSectionName,
                sGlobalShaderCacheParametersReleaseBuildKeyName,
                bIsReleaseBuild);

            auto result = configManager.saveFile(shaderParamsPath, false);
            if (result.has_value()) {
                result->addEntry();
                return result.value();
            }
        }

        // Add a new promise for this new compilation thread.
        vRunningCompilationThreads.push_back(std::promise<bool>());

        auto handle = std::thread(
            &ShaderManager::compileShadersThread,
            this,
            &vRunningCompilationThreads.back(),
            std::move(vShadersToCompile),
            onProgress,
            onError,
            onCompleted);
        handle.detach();

        return {};
    }

    void ShaderManager::compileShadersThread(
        std::promise<bool>* pPromiseFinish,
        const std::vector<ShaderDescription>& vShadersToCompile,
        const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
        const std::function<
            void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
        const std::function<void()>& onCompleted) {
        size_t iThreadId = std::hash<std::thread::id>()(std::this_thread::get_id());

        for (size_t i = 0; i < vShadersToCompile.size(); i++) {
            // Check if shutting down.
            if (bIsShuttingDown.test(std::memory_order_seq_cst)) {
                pPromiseFinish->set_value(true);
                Logger::get().info(
                    std::format(
                        "shader compilation thread {}: finishing early "
                        "due to shutdown",
                        iThreadId),
                    sShaderManagerLogCategory);
                return;
            }

            // Compile shader.
            auto result = IShader::compileShader(vShadersToCompile[i], pRenderer);
            if (!std::holds_alternative<std::unique_ptr<IShader>>(result)) {
                if (std::holds_alternative<std::string>(result)) {
                    const auto sShaderError = std::get<std::string>(std::move(result));
                    const ShaderDescription shaderDescription = vShadersToCompile[i]; // NOLINT
                    pRenderer->getGame()->addDeferredTask([onError,
                                                           shaderDescription = std::move(shaderDescription),
                                                           sShaderError = std::move(sShaderError)]() {
                        onError(shaderDescription, sShaderError);
                    });
                } else {
                    auto err = std::get<Error>(std::move(result));
                    err.addEntry();
                    Logger::get().error(
                        std::format(
                            "shader compilation thread {}: "
                            "an error occurred during shader compilation: {}",
                            iThreadId,
                            err.getError()),
                        sShaderManagerLogCategory);
                    const ShaderDescription shaderDescription = vShadersToCompile[i]; // NOLINT
                    pRenderer->getGame()->addDeferredTask(
                        [onError, shaderDescription = std::move(shaderDescription), err]() mutable {
                            onError(shaderDescription, err);
                        });
                }
            } else {
                // Add compiled shader to shader registry.
                std::scoped_lock<std::mutex> guard(mtxRwShaders);

                auto pShader = std::get<std::unique_ptr<IShader>>(std::move(result));

                auto it = shaders.find(vShadersToCompile[i].sShaderName);
                if (it != shaders.end()) {
                    Error err(std::format(
                        "shader with the name {} already added", vShadersToCompile[i].sShaderName));
                    err.showError();
                    throw std::runtime_error(err.getError());
                }

                shaders[vShadersToCompile[i].sShaderName] = std::move(pShader);
            }

            // Mark progress.
            const auto iCompiledShaderCount = i + 1;
            const auto iShadersToCompile = vShadersToCompile.size();
            Logger::get().info(
                std::format(
                    "shader compilation thread {}: "
                    "progress {}/{}",
                    iThreadId,
                    iCompiledShaderCount,
                    iShadersToCompile),
                sShaderManagerLogCategory);
            pRenderer->getGame()->addDeferredTask([onProgress, iCompiledShaderCount, iShadersToCompile]() {
                onProgress(iCompiledShaderCount, iShadersToCompile);
            });
        }

        Logger::get().info(
            std::format(
                "shader compilation thread {}: "
                "finished compiling {} shader(-s)",
                iThreadId,
                vShadersToCompile.size()),
            sShaderManagerLogCategory);
        pRenderer->getGame()->addDeferredTask(onCompleted);

        pPromiseFinish->set_value(true);
    }
} // namespace ne
