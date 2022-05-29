#include "shaders/ShaderManager.h"

// STL.
#include <format>
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
        const auto [first, last] = std::ranges::remove_if(vRunningCompilationThreads, [](auto& future) {
            using namespace std::chrono_literals;
            try {
                const auto status = future.wait_for(1ms);
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
            for (auto& future : vRunningCompilationThreads) {
                try {
                    future.get();
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

    std::optional<std::shared_ptr<IShader>> ShaderManager::getShader(const std::string& sShaderName) {
        std::scoped_lock<std::mutex> guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) {
            return {};
        } else {
            return it->second;
        }
    }

    long ShaderManager::getShaderUseCount(const std::string& sShaderName) {
        std::scoped_lock<std::mutex> guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) [[unlikely]] {
            Logger::get().error(
                std::format("no shader with the name \"{}\" exists", sShaderName), sShaderManagerLogCategory);
            return 0;
        }

        return it->second.use_count();
    }

    void ShaderManager::removeShaderIfMarkedToBeRemoved(const std::string& sShaderName) {
        std::scoped_lock<std::mutex> guard(mtxRwShaders);
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
            Logger::get().error(
                std::format(
                    "THIS IS A BUG, REPORT TO DEVELOPERS: requested to remove marked to be removed "
                    "shader \"{}\" but shader use count is not 1 "
                    "(actual value: {})",
                    sShaderName,
                    iUseCount),
                sShaderManagerLogCategory);
            return;
        }

        compiledShaders.erase(shaderIt);
        vShadersToBeRemoved.erase(it);

        Logger::get().info(
            std::format("marked to be removed shader \"{}\" was removed", sShaderName),
            sShaderManagerLogCategory);
    }

    bool ShaderManager::isShaderNameCanBeUsed(const std::string& sShaderName) {
        std::scoped_lock<std::mutex> guard(mtxRwShaders);
        const auto it = compiledShaders.find(sShaderName);
        if (it == compiledShaders.end()) {
            return true;
        } else {
            return false;
        }
    }

    bool ShaderManager::markShaderToBeRemoved(const std::string& sShaderName) {
        std::scoped_lock<std::mutex> guard(mtxRwShaders);
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
        const auto iDurationInSec =
            duration_cast<seconds>(system_clock::now() - lastSelfValidationCheckTime).count();
        if (iDurationInSec < 600) { // NOLINT: do validation every 10 minutes
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

                sResultText += "[not found shaders]: ";
                for (const auto& sShaderName : vNotFoundShaders) {
                    sResultText += std::format(" \"{}\"", sShaderName);
                }
                sResultText += "\n";

                sResultText += "[can be removed from \"to remove\" shaders (use count 1)]: ";
                for (const auto& sShaderName : vCanBeRemovedFromToBeRemoved) {
                    sResultText += std::format(" \"{}\"", sShaderName);
                }
                sResultText += "\n";

                sResultText += "[can be released shader bytecode]: ";
                for (const auto& sShaderName : vCanBeReleasedShaderBytecode) {
                    sResultText += std::format(" \"{}\"", sShaderName);
                }
                sResultText += "\n";

                return sResultText;
            }
        };

        SelfValidationResults results;

        std::scoped_lock<std::mutex> guard(mtxRwShaders);

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
            if (pShader->isLoadedIntoMemory() && pShader.use_count() == 1) {
                results.vCanBeReleasedShaderBytecode.push_back(sShaderName);
                pShader->releaseBytecodeFromMemory();
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

        lastSelfValidationCheckTime = system_clock::now();
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

        // Check if shutting down.
        if (bIsShuttingDown.test(std::memory_order_seq_cst)) {
            return {};
        }

        std::scoped_lock<std::mutex> guard(mtxRwShaders);

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

        // Remove finished promises of compilation threads.
        const auto [first, last] = std::ranges::remove_if(vRunningCompilationThreads, [](auto& future) {
            using namespace std::chrono_literals;
            try {
                const auto status = future.wait_for(1ms);
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
        bool bUpdateShaderCacheConfig = false;
        if (std::filesystem::exists(shaderParamsPath)) {
            auto result = configManager.loadFile(shaderParamsPath);
            if (result.has_value()) {
                result->addEntry();
                return result.value();
            }

            const auto bOldShaderCacheInRelease = configManager.getValue<bool>(
                sGlobalShaderCacheParametersSectionName,
                sGlobalShaderCacheParametersReleaseBuildKeyName,
                !bIsReleaseBuild);

            if (bOldShaderCacheInRelease != bIsReleaseBuild) {
                Logger::get().info(
                    "clearing shader cache directory because build mode was changed",
                    sShaderManagerLogCategory);
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
                    "global shader cache parameters file was not found, creating a new {} configuration",
                    bIsReleaseBuild ? "release" : "debug"),
                sShaderManagerLogCategory);

            bUpdateShaderCacheConfig = true;
        }

        if (bUpdateShaderCacheConfig) {
            configManager.setValue<bool>(
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
        std::promise<bool> promise;
        vRunningCompilationThreads.push_back(promise.get_future());

        auto handle = std::thread(
            &ShaderManager::compileShadersThread,
            this,
            std::move(promise),
            std::move(vShadersToCompile),
            onProgress,
            onError,
            onCompleted);
        handle.detach();

        return {};
    }

    void ShaderManager::compileShadersThread(
        std::promise<bool> promiseFinish,
        std::vector<ShaderDescription> vShadersToCompile,
        const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
        const std::function<
            void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
        const std::function<void()>& onCompleted) {
        size_t iThreadId = std::hash<std::thread::id>()(std::this_thread::get_id());

        for (size_t i = 0; i < vShadersToCompile.size(); i++) {
            // Compile shader.
            auto result = IShader::compileShader(vShadersToCompile[i], pRenderer);
            if (!std::holds_alternative<std::shared_ptr<IShader>>(result)) {
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
                auto pShader = std::get<std::shared_ptr<IShader>>(std::move(result));

                // Check if shutting down before locking mutex (otherwise a deadlock will occur).
                if (bIsShuttingDown.test(std::memory_order_seq_cst)) {
                    promiseFinish.set_value(true);
                    Logger::get().info(
                        std::format(
                            "shader compilation thread {}: finishing early "
                            "due to shutdown",
                            iThreadId),
                        sShaderManagerLogCategory);
                    return;
                }
                std::scoped_lock<std::mutex> guard(mtxRwShaders);

                auto it = compiledShaders.find(vShadersToCompile[i].sShaderName);
                if (it != compiledShaders.end()) {
                    Error err(std::format(
                        "shader with the name {} already added", vShadersToCompile[i].sShaderName));
                    err.showError();
                    throw std::runtime_error(err.getError());
                }

                compiledShaders[vShadersToCompile[i].sShaderName] = std::move(pShader);
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

        promiseFinish.set_value(true);
    }
} // namespace ne
