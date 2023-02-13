#include "materials/ShaderManager.h"

// Standard.
#include <filesystem>

// Custom.
#include "game/Game.h"
#include "io/Logger.h"
#include "materials/Shader.h"
#include "render/Renderer.h"
#include "io/ConfigManager.h"
#include "misc/ProjectPaths.h"
#include "materials/ShaderFilesystemPaths.hpp"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "hlsl/HlslShader.h"
#endif

// External.
#include "fmt/core.h"

namespace ne {
    ShaderManager::ShaderManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;

        applyConfigurationFromDisk();

        lastSelfValidationCheckTime = std::chrono::steady_clock::now();
    }

    std::optional<std::shared_ptr<ShaderPack>> ShaderManager::getShader(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) {
            return {};
        }

        return it->second;
    }

    void ShaderManager::releaseShaderBytecodeIfNotUsed(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);

        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) [[unlikely]] {
            Logger::get().error(
                fmt::format("no shader with the name \"{}\" exists", sShaderName), sShaderManagerLogCategory);
            return;
        }

        if (it->second.use_count() > 1) {
            // Still used by somebody else.
            return;
        }

        it->second->releaseShaderPackDataFromMemoryIfLoaded();
    }

    void ShaderManager::removeShaderIfMarkedToBeRemoved(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);
        const auto it = std::ranges::find(vShadersToBeRemoved, sShaderName);
        if (it == vShadersToBeRemoved.end()) {
            // Not marked as "to remove".
            return;
        }

        const auto shaderIt = compiledShaders.find(*it);
        if (shaderIt == compiledShaders.end()) [[unlikely]] {
            Logger::get().error(
                fmt::format("no shader with the name \"{}\" exists", sShaderName), sShaderManagerLogCategory);
            return;
        }

        const long iUseCount = shaderIt->second.use_count();
        if (iUseCount > 1) {
            // Still used by somebody else.
            return;
        }

        compiledShaders.erase(shaderIt);
        vShadersToBeRemoved.erase(it);
    }

    void ShaderManager::applyConfigurationFromDisk() {
        const auto configPath = getConfigurationFilePath();

        if (!std::filesystem::exists(configPath)) {
            writeConfigurationToDisk();
            return;
        }

        ConfigManager configManager;
        auto optional = configManager.loadFile(configPath);
        if (optional.has_value()) {
            auto err = std::move(optional.value());
            err.addEntry();
            // don't show message as it's not a critical error
            Logger::get().error(err.getFullErrorMessage(), sShaderManagerLogCategory);
            return;
        }

        constexpr long iSelfValidationInternalMinimum = 15; // NOLINT: don't do self validation too often.
        auto iNewSelfValidationIntervalInMin = configManager.getValue<long>(
            "", sConfigurationSelfValidationIntervalKeyName, iSelfValidationIntervalInMin);
        if (iNewSelfValidationIntervalInMin < iSelfValidationInternalMinimum) {
            iNewSelfValidationIntervalInMin = iSelfValidationInternalMinimum;
        }

        iSelfValidationIntervalInMin = iNewSelfValidationIntervalInMin;

        // Rewrite configuration on disk because we might correct some values.
        writeConfigurationToDisk();
    }

    std::optional<Error> ShaderManager::clearShaderCacheIfNeeded() {
        std::scoped_lock guard(mtxRwShaders);

#if defined(DEBUG)
        constexpr bool bIsReleaseBuild = false;
#else
        constexpr bool bIsReleaseBuild = true;
#endif

        ConfigManager configManager;

        const auto shaderCacheDir = ShaderFilesystemPaths::getPathToShaderCacheDirectory();
        const auto shaderParamsPath =
            ShaderFilesystemPaths::getPathToShaderCacheDirectory() / sGlobalShaderCacheParametersFileName;

        bool bUpdateShaderCacheConfig = false;

        if (std::filesystem::exists(shaderParamsPath)) {
            auto result = configManager.loadFile(shaderParamsPath);
            if (result.has_value()) {
                result->addEntry();
                return result.value();
            }

            // Read parameters from config.
            const auto bOldShaderCacheInRelease =
                configManager.getValue<bool>("", sGlobalShaderCacheReleaseBuildKeyName, !bIsReleaseBuild);
            const auto sOldHlslVsModel =
                configManager.getValue<std::string>("", sGlobalShaderCacheHlslVsModelKeyName, "");
            const auto sOldHlslPsModel =
                configManager.getValue<std::string>("", sGlobalShaderCacheHlslPsModelKeyName, "");
            const auto sOldHlslCsModel =
                configManager.getValue<std::string>("", sGlobalShaderCacheHlslCsModelKeyName, "");

            // Check if build mode changed.
            if (bOldShaderCacheInRelease != bIsReleaseBuild) {
                Logger::get().info(
                    "clearing shader cache directory because build mode was changed",
                    sShaderManagerLogCategory);

                bUpdateShaderCacheConfig = true;
            }

#if defined(WIN32)
            if (!bUpdateShaderCacheConfig && dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
                // Check if vertex shader model changed.
                if (!bUpdateShaderCacheConfig && sOldHlslVsModel != HlslShader::getVertexShaderModel()) {
                    Logger::get().info(
                        "clearing shader cache directory because vertex shader model was changed",
                        sShaderManagerLogCategory);

                    bUpdateShaderCacheConfig = true;
                }
                // Check if pixel shader model changed.
                if (!bUpdateShaderCacheConfig && sOldHlslPsModel != HlslShader::getPixelShaderModel()) {
                    Logger::get().info(
                        "clearing shader cache directory because pixel shader model was changed",
                        sShaderManagerLogCategory);

                    bUpdateShaderCacheConfig = true;
                }
                // Check if compute shader model changed.
                if (!bUpdateShaderCacheConfig && sOldHlslCsModel != HlslShader::getComputeShaderModel()) {
                    Logger::get().info(
                        "clearing shader cache directory because compute shader model was changed",
                        sShaderManagerLogCategory);

                    bUpdateShaderCacheConfig = true;
                }
            }
#elif __linux__
            static_assert(false, "not implemented");
            // TODO: if (!bUpdateShaderCacheConfig && dynamic_cast<VulkanRenderer*>(pRenderer)) ...
#else
            static_assert(false, "not implemented");
#endif
        } else {
            Logger::get().info(
                fmt::format(
                    "global shader cache configuration was not found, creating a new {} configuration",
                    bIsReleaseBuild ? "release" : "debug"),
                sShaderManagerLogCategory);

            bUpdateShaderCacheConfig = true;
        }

        if (bUpdateShaderCacheConfig) {
            if (std::filesystem::exists(shaderCacheDir)) {
                std::filesystem::remove_all(shaderCacheDir);
                std::filesystem::create_directory(shaderCacheDir);
            }

#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
                configManager.setValue<std::string>(
                    "", sGlobalShaderCacheHlslVsModelKeyName, HlslShader::getVertexShaderModel());
                configManager.setValue<std::string>(
                    "", sGlobalShaderCacheHlslPsModelKeyName, HlslShader::getPixelShaderModel());
                configManager.setValue<std::string>(
                    "", sGlobalShaderCacheHlslCsModelKeyName, HlslShader::getComputeShaderModel());
            }
#elif __linux__
            static_assert(false, "not implemented");
            // TODO: if (!bUpdateShaderCacheConfig && dynamic_cast<VulkanRenderer*>(pRenderer)) ...
#else
            static_assert(false, "not implemented");
#endif

            configManager.setValue<bool>("", sGlobalShaderCacheReleaseBuildKeyName, bIsReleaseBuild);

            auto result = configManager.saveFile(shaderParamsPath, false);
            if (result.has_value()) {
                result->addEntry();
                return result.value();
            }
        }

        return {};
    }

    void ShaderManager::writeConfigurationToDisk() const {
        const auto configPath = getConfigurationFilePath();

        ConfigManager configManager;
        configManager.setValue(
            "",
            sConfigurationSelfValidationIntervalKeyName,
            iSelfValidationIntervalInMin,
            "specified in minutes, interval can't be smaller than 15 minutes, for big games this might cause "
            "small framerate drop each time self validation is performed but "
            "this might find errors (if any occurred) and fix them which might result in slightly less RAM "
            "usage");
        auto optional = configManager.saveFile(configPath, false);
        if (optional.has_value()) {
            auto err = std::move(optional.value());
            err.addEntry();
            // don't show message as it's not a critical error
            Logger::get().error(err.getFullErrorMessage(), sShaderManagerLogCategory);
        }
    }

    std::filesystem::path ShaderManager::getConfigurationFilePath() const {
        std::filesystem::path configPath = ProjectPaths::getDirectoryForEngineConfigurationFiles();
        configPath /= sConfigurationFileName;

        // Check extension.
        if (!std::string_view(sConfigurationFileName).ends_with(ConfigManager::getConfigFormatExtension())) {
            configPath += ConfigManager::getConfigFormatExtension();
        }

        return configPath;
    }

    bool ShaderManager::isShaderNameCanBeUsed(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        return it == compiledShaders.end();
    }

    bool ShaderManager::markShaderToBeRemoved(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) {
            Logger::get().warn(
                fmt::format("no shader with the name \"{}\" exists", sShaderName), sShaderManagerLogCategory);
            return false;
        }

        const long iUseCount = it->second.use_count();
        if (iUseCount > 1) {
            const auto toBeRemovedIt = std::ranges::find(vShadersToBeRemoved, sShaderName);
            if (toBeRemovedIt == vShadersToBeRemoved.end()) {
                Logger::get().info(
                    fmt::format(
                        "shader \"{}\" is marked to be removed later (use count: {})",
                        sShaderName,
                        iUseCount),
                    sShaderManagerLogCategory);
                vShadersToBeRemoved.push_back(sShaderName);
            }
            return true;
        }

        compiledShaders.erase(it);

        return false;
    }

    void ShaderManager::performSelfValidation() { // NOLINT
        using namespace std::chrono;
        const auto iDurationInMin =
            duration_cast<minutes>(steady_clock::now() - lastSelfValidationCheckTime).count();
        if (iDurationInMin < iSelfValidationIntervalInMin) {
            return;
        }

        struct SelfValidationResults {
            std::vector<std::string> vNotFoundShaders;
            std::vector<std::string> vRemovedFromToBeRemoved;
            std::vector<std::string> vReleasedShaderBytecode;
            bool isError() const {
                return !vNotFoundShaders.empty() || !vRemovedFromToBeRemoved.empty() ||
                       !vReleasedShaderBytecode.empty();
            }
            std::string toString() const {
                std::string sResultText;

                if (!vNotFoundShaders.empty()) {
                    sResultText += "[removed not found shaders from \"to remove\" array]: ";
                    for (const auto& sShaderName : vNotFoundShaders) {
                        sResultText += fmt::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                if (!vRemovedFromToBeRemoved.empty()) {
                    sResultText += "[removed from \"to remove\" shaders (use count 1)]: ";
                    for (const auto& sShaderName : vRemovedFromToBeRemoved) {
                        sResultText += fmt::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                if (!vReleasedShaderBytecode.empty()) {
                    sResultText += "[released shader bytecode]: ";
                    for (const auto& sShaderName : vReleasedShaderBytecode) {
                        sResultText += fmt::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                return sResultText;
            }
        };

        SelfValidationResults results;

        std::scoped_lock guard(mtxRwShaders);

        Logger::get().info("starting self validation...", sShaderManagerLogCategory);

        const auto start = steady_clock::now();

        // Look what shaders can be removed.
        for (const auto& sShaderToRemove : vShadersToBeRemoved) {
            const auto it = compiledShaders.find(sShaderToRemove);
            if (it == compiledShaders.end()) [[unlikely]] {
                results.vNotFoundShaders.push_back(sShaderToRemove);
            } else if (it->second.use_count() == 1) {
                results.vRemovedFromToBeRemoved.push_back(sShaderToRemove);
            }
        }

        // Erase shaders that were marked to be removed and not referenced by anyone else
        // from compiled shaders array.
        for (const auto& sShaderName : results.vRemovedFromToBeRemoved) {
            auto it = compiledShaders.find(sShaderName);
            if (it != compiledShaders.end()) {
                compiledShaders.erase(it);
            }
        }
        // Remove them from "to be removed" array.
        for (const auto& sShaderNameToRemove : results.vRemovedFromToBeRemoved) {
            for (auto it = vShadersToBeRemoved.begin(); it != vShadersToBeRemoved.end(); ++it) {
                if (*it == sShaderNameToRemove) {
                    vShadersToBeRemoved.erase(it);
                    break;
                }
            }
        }
        // Remove not found shaders from "to be removed" array.
        for (const auto& sShaderNameToRemove : results.vNotFoundShaders) {
            for (auto it = vShadersToBeRemoved.begin(); it != vShadersToBeRemoved.end(); ++it) {
                if (*it == sShaderNameToRemove) {
                    vShadersToBeRemoved.erase(it);
                    break;
                }
            }
        }

        // Check shaders that were needed but no longer used.
        for (const auto& [sShaderName, pShaderPack] : compiledShaders) {
            if (pShaderPack.use_count() == 1) {
                const bool bReleased = !pShaderPack->releaseShaderPackDataFromMemoryIfLoaded();
                if (bReleased) {
                    results.vReleasedShaderBytecode.push_back(sShaderName);
                }
            }
        }

        const auto end = steady_clock::now();

        const auto iTimeTookInMs = duration_cast<milliseconds>(end - start).count();

        if (results.isError()) {
            Logger::get().error(
                fmt::format(
                    "finished self validation (took {} ms), found and fixed the following "
                    "errors:\n"
                    "\n{}",
                    iTimeTookInMs,
                    results.toString()),
                sShaderManagerLogCategory);
        } else {
            Logger::get().info(
                fmt::format("finished self validation (took {} ms): everything is OK", iTimeTookInMs),
                sShaderManagerLogCategory);
        }

        lastSelfValidationCheckTime = steady_clock::now();
    }

    void ShaderManager::setConfigurationForShaders(
        const std::set<ShaderParameter>& configuration, ShaderType shaderType) {
        std::scoped_lock guard(mtxRwShaders);

        for (const auto& [sShaderName, pShader] : compiledShaders) {
            if (pShader->getShaderType() == shaderType) {
                if (pShader->setConfiguration(configuration)) [[unlikely]] {
                    Error error(fmt::format(
                        "failed to set the shader configuration for the shader \"{}\"",
                        pShader->getShaderName()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }
            }
        }
    }

    std::optional<Error> ShaderManager::compileShaders(
        std::vector<ShaderDescription>& vShadersToCompile,
        const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
        const std::function<
            void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
        const std::function<void()>& onCompleted) {
        if (vShadersToCompile.empty()) [[unlikely]] {
            return Error("the specified array of shaders to compile is empty");
        }

        // Check shader name for forbidden characters and see if source file exists.
        for (const auto& shader : vShadersToCompile) {
            if (shader.sShaderName.size() > iMaximumShaderNameLength) {
                return Error(fmt::format(
                    "shader name \"{}\" is too long (only {} characters allowed)",
                    shader.sShaderName,
                    iMaximumShaderNameLength));
            }
            if (!std::filesystem::exists(shader.pathToShaderFile)) {
                return Error(fmt::format(
                    "shader source file \"{}\" does not exist", shader.pathToShaderFile.string()));
            }
            if (shader.sShaderName.ends_with(" ") || shader.sShaderName.ends_with(".")) {
                return Error(
                    fmt::format("shader name \"{}\" must not end with a dot or a space", shader.sShaderName));
            }
            for (const auto& character : shader.sShaderName) {
                auto it = std::ranges::find(vValidCharactersForShaderName, character);
                if (it == vValidCharactersForShaderName.end()) {
                    return Error(fmt::format(
                        "shader name \"{}\" contains forbidden "
                        "character ({})",
                        shader.sShaderName,
                        character));
                }
            }
        }

        std::scoped_lock guard(mtxRwShaders);

        // Check if we already have a shader with this name.
        for (const auto& shader : vShadersToCompile) {
            if (shader.sShaderName.starts_with(".")) [[unlikely]] {
                return Error(fmt::format(
                    "shader names that start with a dot (\".\") could not be used as "
                    "these files are reserved for internal purposes",
                    sGlobalShaderCacheParametersFileName));
            }

            auto it = compiledShaders.find(shader.sShaderName);
            if (it != compiledShaders.end()) [[unlikely]] {
                return Error(fmt::format(
                    "a shader with the name \"{}\" was already added, "
                    "please choose another name for this shader",
                    shader.sShaderName));
            }
        }

        const auto optional = clearShaderCacheIfNeeded();
        if (optional.has_value()) {
            return optional.value();
        }

        const auto iCurrentQueryId = iTotalCompileShadersQueries.fetch_add(1);
        const auto iTotalShaderCount = vShadersToCompile.size();
        std::shared_ptr<std::atomic<size_t>> pCompiledShaderCount = std::make_shared<std::atomic<size_t>>();

        for (size_t i = 0; i < vShadersToCompile.size(); i++) {
            pRenderer->getGame()->addTaskToThreadPool([this,
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
        std::shared_ptr<ShaderPack> pShaderPack = nullptr;

        // See if we compiled this shader before (check cache).
        if (std::filesystem::exists(
                ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName)) {
            // Try to use cache.
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = ShaderPack::createFromCache(pRenderer, shaderDescription, cacheInvalidationReason);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();

                // Not a critical error.
                if (cacheInvalidationReason.has_value()) {
                    // Invalidated cache.
                    Logger::get().info(err.getInitialMessage(), sShaderManagerLogCategory);
                } else {
                    // Cache files are corrupted.
                    Logger::get().info(
                        fmt::format(
                            "shader \"{}\" cache files are corrupted, attempting to recompile",
                            shaderDescription.sShaderName),
                        sShaderManagerLogCategory);
                }
            } else {
                pShaderPack = std::get<std::shared_ptr<ShaderPack>>(result);
            }
        }

        if (pShaderPack == nullptr) {
            // Compile shader.
            auto result = ShaderPack::compileShaderPack(pRenderer, shaderDescription);
            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(result)) {
                if (std::holds_alternative<std::string>(result)) {
                    auto sShaderError = std::get<std::string>(std::move(result));
                    pRenderer->getGame()->addDeferredTask(
                        [onError, shaderDescription, sShaderError = std::move(sShaderError)]() mutable {
                            onError(std::move(shaderDescription), sShaderError);
                        });
                } else {
                    auto err = std::get<Error>(std::move(result));
                    err.addEntry();
                    Logger::get().error(
                        fmt::format(
                            "shader compilation query #{}: "
                            "an error occurred during shader compilation: {}",
                            iQueryId,
                            err.getFullErrorMessage()),
                        sShaderManagerLogCategory);
                    pRenderer->getGame()->addDeferredTask([onError, shaderDescription, err]() mutable {
                        onError(std::move(shaderDescription), err);
                    });
                }
            } else {
                pShaderPack = std::get<std::shared_ptr<ShaderPack>>(std::move(result));
            }
        }

        if (pShaderPack != nullptr) {
            // Add shader to shader registry.
            std::scoped_lock guard(mtxRwShaders);

            const auto it = compiledShaders.find(shaderDescription.sShaderName);
            if (it != compiledShaders.end()) [[unlikely]] {
                Error err(fmt::format(
                    "shader with the name \"{}\" is already added", shaderDescription.sShaderName));
                Logger::get().error(
                    fmt::format("shader compilation query #{}: {}", iQueryId, err.getFullErrorMessage()),
                    sShaderManagerLogCategory);
                pRenderer->getGame()->addDeferredTask([onError, shaderDescription, err]() mutable {
                    onError(std::move(shaderDescription), err);
                });
            } else {
                // Set initial shader configuration.
                const auto pShaderConfiguration = pRenderer->getShaderConfiguration();
                std::scoped_lock shaderConfigurationGuard(pShaderConfiguration->first);

                bool bFailed = false;
                switch (pShaderPack->getShaderType()) {
                case (ShaderType::VERTEX_SHADER): {
                    bFailed = pShaderPack->setConfiguration(
                        pShaderConfiguration->second->currentVertexShaderConfiguration);
                    break;
                }
                case (ShaderType::PIXEL_SHADER): {
                    bFailed = pShaderPack->setConfiguration(
                        pShaderConfiguration->second->currentPixelShaderConfiguration);
                    break;
                }
                case (ShaderType::COMPUTE_SHADER): {
                    bFailed = true;
                    break;
                }
                }

                if (bFailed) [[unlikely]] {
                    Error err(fmt::format(
                        "failed to set the initial shader configuration for the shader \"{}\"",
                        shaderDescription.sShaderName));
                    Logger::get().error(
                        fmt::format("shader compilation query #{}: {}", iQueryId, err.getFullErrorMessage()),
                        sShaderManagerLogCategory);
                    pRenderer->getGame()->addDeferredTask([onError, shaderDescription, err]() mutable {
                        onError(std::move(shaderDescription), err);
                    });
                }

                // Save shader.
                compiledShaders[shaderDescription.sShaderName] = std::move(pShaderPack);
            }
        }

        // Mark progress.
        const size_t iLastFetchCompiledShaderCount = pCompiledShaderCount->fetch_add(1);

        const auto iCompiledShaderCount = iLastFetchCompiledShaderCount + 1;
        Logger::get().info(
            fmt::format(
                "shader compilation query #{}: "
                "progress {}/{} ({})",
                iQueryId,
                iCompiledShaderCount,
                iTotalShaderCount,
                shaderDescription.sShaderName),
            sShaderManagerLogCategory);
        pRenderer->getGame()->addDeferredTask([onProgress, iCompiledShaderCount, iTotalShaderCount]() {
            onProgress(iCompiledShaderCount, iTotalShaderCount);
        });

        // Make sure that only one task should call onCompleted.
        if (iLastFetchCompiledShaderCount + 1 == iTotalShaderCount) {
            Logger::get().info(
                fmt::format(
                    "shader compilation query #{}: "
                    "finished compiling {} shader(s)",
                    iQueryId,
                    iTotalShaderCount),
                sShaderManagerLogCategory);
            pRenderer->getGame()->addDeferredTask(onCompleted);
        }
    }
} // namespace ne
