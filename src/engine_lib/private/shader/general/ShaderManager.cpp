#include "shader/ShaderManager.h"

// Standard.
#include <filesystem>
#include <format>
#include <chrono>

// Custom.
#include "game/GameManager.h"
#include "io/Logger.h"
#include "shader/general/Shader.h"
#include "render/Renderer.h"
#include "shader/general/ShaderFilesystemPaths.hpp"
#include "cache/ShaderCacheManager.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shader/hlsl/HlslShader.h"
#endif

namespace ne {
    ShaderManager::ShaderManager(Renderer* pRenderer) : pRenderer(pRenderer) {
        pShaderCacheManager = ShaderCacheManager::create(pRenderer);
    }

    std::optional<std::shared_ptr<ShaderPack>> ShaderManager::getShader(const std::string& sShaderName) {
        std::scoped_lock guard(mtxShaderData.first);

        // Find the specified shader name in the global array of shaders.
        const auto it = mtxShaderData.second.compiledShaders.find(sShaderName);
        if (it == mtxShaderData.second.compiledShaders.end()) {
            return {};
        }

        return it->second;
    }

    void ShaderManager::releaseShaderBytecodeIfNotUsed(const std::string& sShaderName) {
        std::scoped_lock guard(mtxShaderData.first);

        // Find the specified shader name in the global array of shaders.
        const auto it = mtxShaderData.second.compiledShaders.find(sShaderName);
        if (it == mtxShaderData.second.compiledShaders.end()) [[unlikely]] {
            Logger::get().error(std::format("no shader with the name \"{}\" exists", sShaderName));
            return;
        }

        // Check if some ShaderUser is using this shader.
        if (it->second.use_count() > 1) {
            // Shader pack is still used by some ShaderUser.
            return;
        }

        // Release shader data from memory.
        it->second->releaseShaderPackDataFromMemoryIfLoaded();
    }

    void ShaderManager::removeShaderIfMarkedToBeRemoved(const std::string& sShaderName) {
        std::scoped_lock guard(mtxShaderData.first);

        // Find the specified name in the array of shaders to be removed.
        const auto toBeRemovedIt = std::ranges::find(mtxShaderData.second.vShadersToBeRemoved, sShaderName);
        if (toBeRemovedIt == mtxShaderData.second.vShadersToBeRemoved.end()) {
            // Not marked as "to be removed".
            return;
        }

        // Find the specified name in the global array of shaders.
        const auto shaderIt = mtxShaderData.second.compiledShaders.find(*toBeRemovedIt);
        if (shaderIt == mtxShaderData.second.compiledShaders.end()) [[unlikely]] {
            Logger::get().error(std::format("no shader with the name \"{}\" exists", sShaderName));
            return;
        }

        // Everything seems to be good, the shader is found.
        // Check if some ShaderUser is using this shader.
        const long iUseCount = shaderIt->second.use_count();
        if (iUseCount > 1) {
            // Still used by some ShaderUser.
            return;
        }

        // Remove the shader.
        mtxShaderData.second.compiledShaders.erase(shaderIt);
        mtxShaderData.second.vShadersToBeRemoved.erase(toBeRemovedIt);
    }

    std::optional<Error> ShaderManager::refreshShaderCache() {
        std::scoped_lock guard(mtxShaderData.first);

        auto optionalError = pShaderCacheManager->refreshShaderCache();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    bool ShaderManager::isShaderNameCanBeUsed(const std::string& sShaderName) {
        std::scoped_lock guard(mtxShaderData.first);

        const auto it = mtxShaderData.second.compiledShaders.find(sShaderName);

        return it == mtxShaderData.second.compiledShaders.end();
    }

    bool ShaderManager::markShaderToBeRemoved(const std::string& sShaderName) {
        std::scoped_lock guard(mtxShaderData.first);

        // Find the specified shader name in the global array of shaders.
        const auto it = mtxShaderData.second.compiledShaders.find(sShaderName);
        if (it == mtxShaderData.second.compiledShaders.end()) [[unlikely]] {
            Logger::get().warn(std::format("no shader with the name \"{}\" exists", sShaderName));
            return false;
        }

        // Check if some ShaderUser is using this shader.
        const long iUseCount = it->second.use_count();
        if (iUseCount > 1) {
            // Mark the shader as "to be removed".
            const auto toBeRemovedIt =
                std::ranges::find(mtxShaderData.second.vShadersToBeRemoved, sShaderName);
            if (toBeRemovedIt == mtxShaderData.second.vShadersToBeRemoved.end()) {
                Logger::get().info(std::format(
                    "shader \"{}\" is marked to be removed later (use count: {})", sShaderName, iUseCount));
                mtxShaderData.second.vShadersToBeRemoved.push_back(sShaderName);
            }
            return true;
        }

        // Remove the shader.
        mtxShaderData.second.compiledShaders.erase(it);

        return false;
    }

    void ShaderManager::performSelfValidation() { // NOLINT
        std::scoped_lock guard(mtxShaderData.first);

        struct SelfValidationResults {
            std::vector<std::string> vNotFoundShaders;
            std::vector<std::string> vRemovedFromToBeRemoved;
            bool isError() const { return !vNotFoundShaders.empty() || !vRemovedFromToBeRemoved.empty(); }
            std::string toString() const {
                std::string sResultText;

                if (!vNotFoundShaders.empty()) {
                    sResultText += "[removed not found shaders from \"to remove\" array]: ";
                    for (const auto& sShaderName : vNotFoundShaders) {
                        sResultText += std::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                if (!vRemovedFromToBeRemoved.empty()) {
                    sResultText += "[removed from \"to remove\" shaders (use count 1)]: ";
                    for (const auto& sShaderName : vRemovedFromToBeRemoved) {
                        sResultText += std::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                return sResultText;
            }
        };

        SelfValidationResults results;

        Logger::get().info("starting self validation...");

        const auto start = std::chrono::steady_clock::now();

        // Look what shaders can be removed.
        for (const auto& sShaderToRemove : mtxShaderData.second.vShadersToBeRemoved) {
            const auto it = mtxShaderData.second.compiledShaders.find(sShaderToRemove);
            if (it == mtxShaderData.second.compiledShaders.end()) [[unlikely]] {
                results.vNotFoundShaders.push_back(sShaderToRemove);
            } else if (it->second.use_count() == 1) {
                results.vRemovedFromToBeRemoved.push_back(sShaderToRemove);
            }
        }

        // Erase shaders that were marked to be removed and not referenced by anyone else
        // from compiled shaders array.
        for (const auto& sShaderName : results.vRemovedFromToBeRemoved) {
            auto it = mtxShaderData.second.compiledShaders.find(sShaderName);
            if (it != mtxShaderData.second.compiledShaders.end()) {
                mtxShaderData.second.compiledShaders.erase(it);
            }
        }
        // Remove them from "to be removed" array.
        for (const auto& sShaderNameToRemove : results.vRemovedFromToBeRemoved) {
            for (auto it = mtxShaderData.second.vShadersToBeRemoved.begin();
                 it != mtxShaderData.second.vShadersToBeRemoved.end();
                 ++it) {
                if (*it == sShaderNameToRemove) {
                    mtxShaderData.second.vShadersToBeRemoved.erase(it);
                    break;
                }
            }
        }
        // Remove not found shaders from "to be removed" array.
        for (const auto& sShaderNameToRemove : results.vNotFoundShaders) {
            for (auto it = mtxShaderData.second.vShadersToBeRemoved.begin();
                 it != mtxShaderData.second.vShadersToBeRemoved.end();
                 ++it) {
                if (*it == sShaderNameToRemove) {
                    mtxShaderData.second.vShadersToBeRemoved.erase(it);
                    break;
                }
            }
        }

        // Measure the time it took to run garbage collector.
        const auto end = std::chrono::steady_clock::now();
        const auto timeTookInMs =
            std::chrono::duration<float, std::chrono::milliseconds::period>(end - start).count();

        // Log results.
        if (results.isError()) {
            Logger::get().error(std::format(
                "finished self validation (took {:.1F} ms), found and fixed the following "
                "errors: (this should not happen)\n"
                "\n{}",
                timeTookInMs,
                results.toString()));
        } else {
            Logger::get().info(
                std::format("finished self validation (took {:.1F} ms): everything is OK", timeTookInMs));
        }
    }

    void ShaderManager::setRendererConfigurationForShaders(
        const std::set<ShaderMacro>& configuration, ShaderType shaderType) {
        std::scoped_lock guard(mtxShaderData.first);

        for (const auto& [sShaderName, pShader] : mtxShaderData.second.compiledShaders) {
            if (pShader->getShaderType() == shaderType) {
                pShader->setRendererConfiguration(configuration);
            }
        }
    }

    std::optional<Error> ShaderManager::compileShaders(
        std::vector<ShaderDescription> vShadersToCompile,
        const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
        const std::function<
            void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
        const std::function<void()>& onCompleted) {
        if (vShadersToCompile.empty()) [[unlikely]] {
            return Error("the specified array of shaders to compile is empty");
        }

        // Check shader name for forbidden characters and see if source file exists.
        for (const auto& shader : vShadersToCompile) {
            if (shader.sShaderName.size() > iMaximumShaderNameLength) [[unlikely]] {
                return Error(std::format(
                    "shader name \"{}\" is too long (only {} characters allowed)",
                    shader.sShaderName,
                    iMaximumShaderNameLength));
            }
            if (!std::filesystem::exists(shader.pathToShaderFile)) [[unlikely]] {
                return Error(std::format(
                    "shader source file \"{}\" does not exist", shader.pathToShaderFile.string()));
            }
            if (shader.sShaderName.ends_with(" ") || shader.sShaderName.ends_with(".")) [[unlikely]] {
                return Error(
                    std::format("shader name \"{}\" must not end with a dot or a space", shader.sShaderName));
            }
            for (const auto& character : shader.sShaderName) {
                const auto it = // NOLINT: invalid case for `it`
                    std::ranges::find(vValidCharactersForShaderName, character);
                if (it == vValidCharactersForShaderName.end()) {
                    return Error(std::format(
                        "shader name \"{}\" contains forbidden character \"{}\"",
                        shader.sShaderName,
                        character));
                }
            }
        }

        std::scoped_lock guard(mtxShaderData.first);

        // Check if we already have a shader with this name.
        for (const auto& shader : vShadersToCompile) {
            if (shader.sShaderName.starts_with(".")) [[unlikely]] {
                return Error("shader names that start with a dot (\".\") could not be used as "
                             "these names are reserved for internal purposes");
            }

            const auto it = mtxShaderData.second.compiledShaders.find(shader.sShaderName);
            if (it != mtxShaderData.second.compiledShaders.end()) [[unlikely]] {
                return Error(std::format(
                    "a shader with the name \"{}\" was already added, "
                    "please choose another name for this shader",
                    shader.sShaderName));
            }
        }

        // Prepare for shader compilation.
        const auto iCurrentQueryId = iTotalCompileShadersQueries.fetch_add(1);
        const auto iTotalShaderCount = vShadersToCompile.size();
        std::shared_ptr<std::atomic<size_t>> pCompiledShaderCount = std::make_shared<std::atomic<size_t>>();

        // Start compilation tasks.
        for (size_t i = 0; i < vShadersToCompile.size(); i++) {
            pRenderer->getGameManager()->addTaskToThreadPool(
                [this,
                 iCurrentQueryId,
                 pCompiledShaderCount,
                 iTotalShaderCount,
                 shaderToCompile = std::move(vShadersToCompile[i]),
                 onProgress,
                 onError,
                 onCompleted]() mutable {
                    compileShaderTask(
                        iCurrentQueryId,
                        pCompiledShaderCount,
                        iTotalShaderCount,
                        std::move(shaderToCompile),
                        onProgress,
                        onError,
                        onCompleted);
                });
        }

        return {};
    }

    void ShaderManager::compileShaderTask(
        size_t iQueryId,
        const std::shared_ptr<std::atomic<size_t>>& pCompiledShaderCount,
        size_t iTotalShaderCount,
        ShaderDescription shaderDescription,
        const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
        const std::function<
            void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
        const std::function<void()>& onCompleted) {
        // Prepare pointer to shader pack.
        std::shared_ptr<ShaderPack> pShaderPack = nullptr;

        // See if this shader is in the shader cache on disk (was compiled before and still valid).
        if (std::filesystem::exists(
                ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName)) {
            // Try to use cached data.
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = ShaderPack::createFromCache(pRenderer, shaderDescription, cacheInvalidationReason);
            if (std::holds_alternative<Error>(result)) {
                // Cache is corrupted / invalid.
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();

                // Not a critical error.
                if (cacheInvalidationReason.has_value()) {
                    // Cache was invalidated. Log information about invalidated cache.
                    Logger::get().info(err.getInitialMessage());
                } else {
                    // Cache files are corrupted/outdated. Need recompilation.
                    Logger::get().info(std::format(
                        "shader \"{}\" cache files are corrupted/outdated, attempting to recompile",
                        shaderDescription.sShaderName));
                }
            } else {
                // Save shader found in cache.
                pShaderPack = std::get<std::shared_ptr<ShaderPack>>(result);
            }
        }

        if (pShaderPack == nullptr) {
            // Compile shader.
            auto result = ShaderPack::compileShaderPack(pRenderer, shaderDescription);
            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(result)) {
                if (std::holds_alternative<std::string>(result)) {
                    // Shader compilation error/warning.
                    auto sShaderError = std::get<std::string>(std::move(result));
                    pRenderer->getGameManager()->addDeferredTask(
                        [onError, shaderDescription, sShaderError = std::move(sShaderError)]() mutable {
                            onError(std::move(shaderDescription), sShaderError);
                        });
                } else {
                    // Internal error.
                    auto err = std::get<Error>(std::move(result));
                    err.addCurrentLocationToErrorStack();
                    Logger::get().error(std::format(
                        "shader compilation query #{}: "
                        "an error occurred during shader compilation: {}",
                        iQueryId,
                        err.getFullErrorMessage()));
                    pRenderer->getGameManager()->addDeferredTask([onError, shaderDescription, err]() mutable {
                        onError(std::move(shaderDescription), err);
                    });
                }
            } else {
                // Save compiled shader.
                pShaderPack = std::get<std::shared_ptr<ShaderPack>>(std::move(result));
            }
        }

        if (pShaderPack != nullptr) {
            // Add shader to the shader registry.
            std::scoped_lock guard(mtxShaderData.first);

            // Make sure the shader registry does not have a shader with this name.
            const auto it = mtxShaderData.second.compiledShaders.find(shaderDescription.sShaderName);
            if (it != mtxShaderData.second.compiledShaders.end()) [[unlikely]] {
                Error err(std::format(
                    "shader with the name \"{}\" is already added", shaderDescription.sShaderName));
                Logger::get().error(
                    std::format("shader compilation query #{}: {}", iQueryId, err.getFullErrorMessage()));
                pRenderer->getGameManager()->addDeferredTask([onError, shaderDescription, err]() mutable {
                    onError(std::move(shaderDescription), err);
                });
            } else {
                // Set initial shader configuration.
                const auto pShaderConfiguration = pRenderer->getShaderConfiguration();
                std::scoped_lock shaderConfigurationGuard(pShaderConfiguration->first);

                switch (pShaderPack->getShaderType()) {
                case (ShaderType::VERTEX_SHADER): {
                    pShaderPack->setRendererConfiguration(
                        pShaderConfiguration->second->currentVertexShaderConfiguration);
                    break;
                }
                case (ShaderType::FRAGMENT_SHADER): {
                    pShaderPack->setRendererConfiguration(
                        pShaderConfiguration->second->currentPixelShaderConfiguration);
                    break;
                }
                case (ShaderType::COMPUTE_SHADER): {
                    pShaderPack->setRendererConfiguration({});
                    break;
                }
                }

                // Save shader to shader registry.
                mtxShaderData.second.compiledShaders[shaderDescription.sShaderName] = std::move(pShaderPack);
            }
        }

        // Mark progress.
        const size_t iLastFetchCompiledShaderCount = pCompiledShaderCount->fetch_add(1);
        const auto iCompiledShaderCount = iLastFetchCompiledShaderCount + 1;
        Logger::get().info(std::format(
            "shader compilation query #{}: "
            "progress {}/{} ({})",
            iQueryId,
            iCompiledShaderCount,
            iTotalShaderCount,
            shaderDescription.sShaderName));
        pRenderer->getGameManager()->addDeferredTask([onProgress, iCompiledShaderCount, iTotalShaderCount]() {
            onProgress(iCompiledShaderCount, iTotalShaderCount);
        });

        // Make sure that only one task would call `onCompleted` callback.
        if (iLastFetchCompiledShaderCount + 1 == iTotalShaderCount) {
            Logger::get().info(std::format(
                "shader compilation query #{}: "
                "finished compiling {} shader(s)",
                iQueryId,
                iTotalShaderCount));
            pRenderer->getGameManager()->addDeferredTask(onCompleted);
        }
    }
} // namespace ne
