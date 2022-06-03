#include "shaders/ShaderManager.h"

// STL.
#include <format>
#include <filesystem>

// Custom.
#include "game/Game.h"
#include "io/Logger.h"
#include "shaders/IShader.h"
#include "render/IRenderer.h"
#include "io/ConfigManager.h"
#include "misc/Globals.h"

namespace ne {
    ShaderManager::ShaderManager(IRenderer* pRenderer) {
        this->pRenderer = pRenderer;

        applyConfigurationFromDisk();

        lastSelfValidationCheckTime = std::chrono::steady_clock::now();
    }

    std::optional<std::shared_ptr<IShader>> ShaderManager::getShader(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) {
            return {};
        } else {
            return it->second;
        }
    }

    void ShaderManager::releaseShaderBytecodeIfNotUsed(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);

        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) [[unlikely]] {
            Logger::get().error(
                std::format("no shader with the name \"{}\" exists", sShaderName), sShaderManagerLogCategory);
            return;
        }

        if (it->second.use_count() > 1) {
            return;
        }

        it->second->releaseBytecodeFromMemoryIfLoaded();
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
                std::format("no shader with the name \"{}\" exists", sShaderName), sShaderManagerLogCategory);
            return;
        }

        const long iUseCount = shaderIt->second.use_count();
        if (iUseCount > 1) {
            // Still used by somebody else.
            return;
        }

        compiledShaders.erase(shaderIt);
        vShadersToBeRemoved.erase(it);

        Logger::get().info(
            std::format("marked to be removed shader \"{}\" was removed", sShaderName),
            sShaderManagerLogCategory);
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
            Logger::get().error(err.getError(), sShaderManagerLogCategory);
            return;
        }

        auto iNewSelfValidationIntervalInMin = configManager.getValue<long>(
            "", sConfigurationSelfValidationIntervalKeyName, iSelfValidationIntervalInMin);
        if (iNewSelfValidationIntervalInMin < 15) { // NOLINT: don't do self validation too often.
            iNewSelfValidationIntervalInMin = 15;
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

        const std::string sCurrentGpuName = pRenderer->getCurrentlyUsedGpuName();

        ConfigManager configManager;

        const auto shaderCacheDir = IShader::getPathToShaderCacheDirectory();
        const auto shaderParamsPath =
            IShader::getPathToShaderCacheDirectory() / sGlobalShaderCacheParametersFileName;

        bool bUpdateShaderCacheConfig = false;

        if (std::filesystem::exists(shaderParamsPath)) {
            auto result = configManager.loadFile(shaderParamsPath);
            if (result.has_value()) {
                result->addEntry();
                return result.value();
            }

            const auto bOldShaderCacheInRelease =
                configManager.getValue<bool>("", sGlobalShaderCacheReleaseBuildKeyName, !bIsReleaseBuild);
            const auto sOldGpuName =
                configManager.getValue<std::string>("", sGlobalShaderCacheGpuKeyName, "");

            if (bOldShaderCacheInRelease != bIsReleaseBuild || sOldGpuName != sCurrentGpuName) {
                if (bOldShaderCacheInRelease != bIsReleaseBuild) {
                    Logger::get().info(
                        "clearing shader cache directory because build mode was changed",
                        sShaderManagerLogCategory);
                } else {
                    Logger::get().info(
                        "clearing shader cache directory because GPU was changed", sShaderManagerLogCategory);
                }

                std::filesystem::remove_all(shaderCacheDir);
                std::filesystem::create_directory(shaderCacheDir);

                bUpdateShaderCacheConfig = true;
            }
        } else {
            if (std::filesystem::exists(shaderCacheDir)) {
                std::filesystem::remove_all(shaderCacheDir);
                std::filesystem::create_directory(shaderCacheDir);
            }

            Logger::get().info(
                std::format(
                    "global shader cache configuration was not found, creating a new {} configuration",
                    bIsReleaseBuild ? "release" : "debug"),
                sShaderManagerLogCategory);

            bUpdateShaderCacheConfig = true;
        }

        if (bUpdateShaderCacheConfig) {
            configManager.setValue<bool>("", sGlobalShaderCacheReleaseBuildKeyName, bIsReleaseBuild);
            configManager.setValue<std::string>("", sGlobalShaderCacheGpuKeyName, sCurrentGpuName);

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
            Logger::get().error(err.getError(), sShaderManagerLogCategory);
        }
    }

    std::filesystem::path ShaderManager::getConfigurationFilePath() const {
        std::filesystem::path configPath = getBaseDirectoryForConfigs();
        configPath += getApplicationName();

        if (!std::filesystem::exists(configPath)) {
            std::filesystem::create_directory(configPath);
        }

        configPath /= sEngineDirectoryName;

        if (!std::filesystem::exists(configPath)) {
            std::filesystem::create_directory(configPath);
        }

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
        if (it == compiledShaders.end()) {
            return true;
        } else {
            return false;
        }
    }

    bool ShaderManager::markShaderToBeRemoved(const std::string& sShaderName) {
        std::scoped_lock guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) {
            Logger::get().warn(
                std::format("no shader with the name \"{}\" exists", sShaderName), sShaderManagerLogCategory);
            return false;
        }

        const long iUseCount = it->second.use_count();
        if (iUseCount > 1) {
            const auto toBeRemovedIt = std::ranges::find(vShadersToBeRemoved, sShaderName);
            if (toBeRemovedIt == vShadersToBeRemoved.end()) {
                Logger::get().info(
                    std::format(
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

    void ShaderManager::performSelfValidation() {
        using namespace std::chrono;
        const auto iDurationInMin =
            duration_cast<minutes>(steady_clock::now() - lastSelfValidationCheckTime).count();
        if (iDurationInMin < iSelfValidationIntervalInMin) {
            return;
        }

        struct SelfValidationResults {
            std::vector<std::string> vNotFoundShaders;
            std::vector<std::string> vCanBeRemovedFromToBeRemoved;
            std::vector<std::string> vCanBeReleasedShaderBytecode;
            bool isError() const {
                return !vNotFoundShaders.empty() || !vCanBeRemovedFromToBeRemoved.empty() ||
                       !vCanBeReleasedShaderBytecode.empty();
            }
            std::string toString() const {
                std::string sResultText;

                if (!vNotFoundShaders.empty()) {
                    sResultText += "[removed not found shaders]: ";
                    for (const auto& sShaderName : vNotFoundShaders) {
                        sResultText += std::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                if (!vCanBeRemovedFromToBeRemoved.empty()) {
                    sResultText += "[removed from \"to remove\" shaders (use count 1)]: ";
                    for (const auto& sShaderName : vCanBeRemovedFromToBeRemoved) {
                        sResultText += std::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                if (!vCanBeReleasedShaderBytecode.empty()) {
                    sResultText += "[released shader bytecode]: ";
                    for (const auto& sShaderName : vCanBeReleasedShaderBytecode) {
                        sResultText += std::format(" \"{}\"", sShaderName);
                    }
                    sResultText += "\n";
                }

                return sResultText;
            }
        };

        SelfValidationResults results;

        std::scoped_lock guard(mtxRwShaders);

        Logger::get().info("starting ShaderManager self validation...", sShaderManagerLogCategory);

        const auto start = steady_clock::now();

        // Check "to remove" shaders.
        const auto [first, last] =
            std::ranges::remove_if(vShadersToBeRemoved, [this, &results](const auto& sShaderToRemove) {
                const auto it = compiledShaders.find(sShaderToRemove);
                if (it == compiledShaders.end()) [[unlikely]] {
                    results.vNotFoundShaders.push_back(sShaderToRemove);
                    return true;
                }

                if (it->second.use_count() == 1) {
                    results.vCanBeRemovedFromToBeRemoved.push_back(sShaderToRemove);
                    return true;
                }

                return false;
            });
        for (auto it = first; it != last; ++it) {
            auto shaderIt = compiledShaders.find(*it);
            if (shaderIt != compiledShaders.end()) {
                compiledShaders.erase(shaderIt);
            }
        }
        vShadersToBeRemoved.erase(first, last);

        // Check shaders that were needed but no longer used.
        for (const auto& [sShaderName, pShader] : compiledShaders) {
            if (pShader.use_count() == 1) {
                const bool bReleased = !pShader->releaseBytecodeFromMemoryIfLoaded();
                if (bReleased) {
                    results.vCanBeReleasedShaderBytecode.push_back(sShaderName);
                }
            }
        }

        const auto end = steady_clock::now();

        const auto iTimeTookInMs = duration_cast<milliseconds>(end - start).count();

        if (results.isError()) {
            Logger::get().error(
                std::format(
                    "finished ShaderManager self validation (took {} ms), found and fixed the following "
                    "errors:\n"
                    "\n{}",
                    iTimeTookInMs,
                    results.toString()),
                sShaderManagerLogCategory);
        } else {
            Logger::get().info(
                std::format(
                    "finished ShaderManager self validation (took {} ms): everything is OK!", iTimeTookInMs),
                sShaderManagerLogCategory);
        }

        lastSelfValidationCheckTime = steady_clock::now();
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
            if (!std::filesystem::exists(shader.pathToShaderFile)) {
                return Error(std::format(
                    "shader source file \"{}\" does not exist", shader.pathToShaderFile.string()));
            }
            if (shader.sShaderName.ends_with(" ") || shader.sShaderName.ends_with(".")) {
                return Error(
                    std::format("shader name \"{}\" must not end with a dot or a space", shader.sShaderName));
            }
            for (const auto& character : shader.sShaderName) {
                auto it = std::ranges::find(vValidCharactersForShaderName, character);
                if (it == vValidCharactersForShaderName.end()) {
                    return Error(std::format(
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
                return Error(std::format(
                    "shader names that start with a dot (\".\") could not be used as "
                    "these files are reserved for internal purposes",
                    sGlobalShaderCacheParametersFileName));
            }

            auto it = compiledShaders.find(shader.sShaderName);
            if (it != compiledShaders.end()) [[unlikely]] {
                return Error(std::format(
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
        ShaderDescription shaderToCompile,
        const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
        const std::function<
            void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
        const std::function<void()>& onCompleted) {
        // Compile shader.
        auto result = IShader::compileShader(shaderToCompile, pRenderer);
        if (!std::holds_alternative<std::shared_ptr<IShader>>(result)) {
            if (std::holds_alternative<std::string>(result)) {
                const auto sShaderError = std::get<std::string>(std::move(result));
                pRenderer->getGame()->addDeferredTask(
                    [onError, shaderToCompile, sShaderError = std::move(sShaderError)]() mutable {
                        onError(std::move(shaderToCompile), sShaderError);
                    });
            } else {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                Logger::get().error(
                    std::format(
                        "shader compilation query #{}: "
                        "an error occurred during shader compilation: {}",
                        iQueryId,
                        err.getError()),
                    sShaderManagerLogCategory);
                pRenderer->getGame()->addDeferredTask(
                    [onError, shaderToCompile, err]() mutable { onError(std::move(shaderToCompile), err); });
            }
        } else {
            // Add compiled shader to shader registry.
            auto pShader = std::get<std::shared_ptr<IShader>>(std::move(result));

            std::scoped_lock guard(mtxRwShaders);

            const auto it = compiledShaders.find(shaderToCompile.sShaderName);
            if (it != compiledShaders.end()) {
                Error err(
                    std::format("shader with the name \"{}\" is already added", shaderToCompile.sShaderName));
                Logger::get().error(
                    std::format("shader compilation query #{}: {}", iQueryId, err.getError()),
                    sShaderManagerLogCategory);
                pRenderer->getGame()->addDeferredTask(
                    [onError, shaderToCompile, err]() mutable { onError(std::move(shaderToCompile), err); });
            } else {
                compiledShaders[shaderToCompile.sShaderName] = std::move(pShader);
            }
        }

        const size_t iLastFetchCompiledShaderCount = pCompiledShaderCount->fetch_add(1);

        // Mark progress.
        const auto iCompiledShaderCount = iLastFetchCompiledShaderCount + 1;
        Logger::get().info(
            std::format(
                "shader compilation query #{}: "
                "progress {}/{}",
                iQueryId,
                iCompiledShaderCount,
                iTotalShaderCount),
            sShaderManagerLogCategory);
        pRenderer->getGame()->addDeferredTask([onProgress, iCompiledShaderCount, iTotalShaderCount]() {
            onProgress(iCompiledShaderCount, iTotalShaderCount);
        });

        // Only one task should call onCompleted.
        if (iLastFetchCompiledShaderCount + 1 == iTotalShaderCount) {
            Logger::get().info(
                std::format(
                    "shader compilation query #{}: "
                    "finished compiling {} shader(-s)",
                    iQueryId,
                    iTotalShaderCount),
                sShaderManagerLogCategory);
            pRenderer->getGame()->addDeferredTask(onCompleted);
        }
    }
} // namespace ne
