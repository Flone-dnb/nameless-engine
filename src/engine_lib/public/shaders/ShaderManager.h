#pragma once

// STL.
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <future>
#include <atomic>
#include <variant>
#include <array>
#include <chrono>

// Custom.
#include "misc/Error.h"
#include "shaders/ShaderDescription.h"

namespace ne {
    constexpr auto sShaderManagerLogCategory = "Shader Manager";

    class IShader;
    class IRenderer;

    /**
     * Controls shader compilation, shader registry.
     */
    class ShaderManager {
    public:
        /**
         * Constructor.
         *
         * @param pRenderer Parent renderer that uses this shader manager.
         */
        ShaderManager(IRenderer* pRenderer);

        ShaderManager() = delete;
        ShaderManager(const ShaderManager&) = delete;
        ShaderManager& operator=(const ShaderManager&) = delete;

        virtual ~ShaderManager();

        /**
         * Add shaders to be asynchronously compiled.
         *
         * Compiled shaders are kept on disk, once a shader is needed it will be
         * loaded from disk into memory.
         *
         * @param vShadersToCompile Array of shaders to compile. Use @ref isShaderNameCanBeUsed
         * to check if a shader name is free (unique).
         * @param onProgress        Callback function that will be called when each shader is compiled.
         * This will also be called when all shaders are compiled (together with 'onCompleted').
         * The first argument is number of compiled shaders and the second one is total number of
         * shaders to compile.
         * @param onError           Callback function that will be called if an error occurred.
         * This might be one of the two things: shader compilation error/warning (shader contains
         * error) or internal error (engine failed to compile shader).
         * If there was a shader compilation error/warning, this shader will be marked as processed and
         * onProgress will be called (but this shader will not be added to shader manager and will
         * not be available, use will need to fix the error and add this shader again).
         * @param onCompleted       Callback function that will be called once all shaders are compiled.
         *
         * @warning Note that all callback functions will be queued to be executed on the main thread and
         * will be called later from the main thread before next frame is rendered.
         * If you are using member functions as callbacks you need to make sure that the owner object
         * of these member functions will not be deleted until onCompleted is called.
         * Because callbacks are called from the main thread it's safe to call functions that are
         * marked as "should only be called from the main thread" from the callback functions.
         *
         * @return An error if something went wrong.
         */
        std::optional<Error> compileShaders(
            const std::vector<ShaderDescription>& vShadersToCompile,
            const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
            const std::function<
                void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
            const std::function<void()>& onCompleted);

        /**
         * Checks if the shader name is free (unique) to be used in @ref compileShaders.
         *
         * @param sShaderName Name to check.
         *
         * @return 'true' if can be used, 'false' otherwise.
         */
        bool isShaderNameCanBeUsed(const std::string& sShaderName);

        /**
         * Removes shader if nobody is referencing it, otherwise marks shader to be removed
         * later.
         *
         * Typically you would not use this function as we expect you to do
         * one call to @ref compileShaders in the beginning of the game to compile
         * ALL of your shaders (for all levels) and never remove them as compiled shaders
         * are not stored in memory, they are stored on disk and when actually needed/used
         * loaded from disk to memory. If some shader was used but no longer needed it will
         * be released from memory until someone will need it again.
         *
         * If somebody is still referencing this shader, the shader will be added to
         * "to remove" array and will be removed later when nobody
         * is referencing this shader (specifically when only one
         * std::shared_ptr<IShader> instance pointing to this shader
         * will exist (it will exist in @ref ShaderManager as @ref ShaderManager
         * stores pointer to each shader). Shaders to be removed are usually checked
         * every minute.
         *
         * @param sShaderName Shader's name to be marked for removal.
         *
         * @return 'true' if someone is still referencing this shader
         * and it cannot be removed right now, thus shader's name still
         * cannot be used in @ref compileShaders. Returns 'false' if
         * nobody was referencing this shader and it was removed, thus
         * shader's name can now be used in @ref compileShaders.
         */
        bool markShaderToBeRemoved(const std::string& sShaderName);

        /**
         * Automatically called by the Game object and has no point in being
         * called from user code.
         *
         * Analyzes current state to see if any errors have place.
         * Fixes errors and reports them in log.
         *
         * @remark A call to this function may be ignored by the ShaderManager if
         * the previous self validation was performed not so long ago.
         */
        void performSelfValidation();

    protected:
        // Only ShaderUser should be able to work with shaders.
        friend class ShaderUser;

        /**
         * Compiles each shader. Usually called in another thread to do this work asynchronously.
         *
         * @param promiseFinish     Promise to set when finished or exited. Do not delete this pointer.
         * @param vShadersToCompile Array of shaders to compile.
         * @param onProgress        Callback function that will be called when each shader is compiled.
         * This will also be called when all shaders are compiled (together with 'onCompleted').
         * The first argument is number of compiled shaders and the second one is total number of
         * shaders to compile.
         * @param onError           Callback function that will be called if an error occurred.
         * This might be one of the two things: shader compilation error/warning (shader contains
         * error) or internal error (engine failed somewhere).
         * If there was a shader compilation error/warning, this shader will be marked as processed and
         * onProgress will be called (but this shader will not be added to shader manager and will
         * not be available, use will need to fix the error and add this shader again).
         * @param onCompleted       Callback function that will be called once all shaders are compiled.
         */
        void compileShadersThread(
            std::promise<bool> promiseFinish,
            std::vector<ShaderDescription> vShadersToCompile,
            const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
            const std::function<
                void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
            const std::function<void()>& onCompleted);

        /**
         * @warning Should only be called by ShaderUser class.
         *
         * Returns compiled shader (compiled using @ref compileShaders).
         *
         * @param sShaderName Name of this shader.
         *
         * @return Empty if the shader with the specified name was not found,
         * valid pointer otherwise.
         */
        std::optional<std::shared_ptr<IShader>> getShader(const std::string& sShaderName);

        /**
         * Looks if the specified shader is not used by anyone and releases shader bytecode
         * from memory if it was previously loaded.
         *
         * @param sShaderName Name of the shader to release bytecode.
         */
        void releaseShaderBytecodeIfNotUsed(const std::string& sShaderName);

        /**
         * Looks if this shader was marked "to be removed" and that it's not being used by anyone else,
         * if this is correct removes the shader.
         *
         * @param sShaderName Name of the shader to remove.
         */
        void removeShaderIfMarkedToBeRemoved(const std::string& sShaderName);

        /**
         * Reads and applies configuration from disk.
         * If any values from disk configuration were invalid/corrected will
         * override old configuration with new/corrected values.
         *
         * @remark If no configuration file existed will create it.
         */
        void applyConfigurationFromDisk();

        /**
         * Writes current configuration to disk.
         */
        void writeConfigurationToDisk() const;

        /**
         * Returns path to the configuration file.
         *
         * @return Path to configuration file.
         */
        std::filesystem::path getConfigurationFilePath() const;

    private:
        /** Do not delete. Parent renderer that uses this shader manager. */
        IRenderer* pRenderer;

        /** Use for @ref compiledShaders and @ref vRunningCompilationThreads. */
        std::mutex mtxRwShaders;

        /**
         * Array of characters that can be used for shader name.
         * We limit amount of valid characters because we store compiled shaders on disk
         * and different filesystems have different limitations for file names.
         */
        const std::array<char, 65> vValidCharactersForShaderName = {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
            'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
            'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
            'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '_', '-'};

        /** Use with @ref mtxRwShaders. Map of compiled (added) shaders. */
        std::unordered_map<std::string, std::shared_ptr<IShader>> compiledShaders;

        /** Use with @ref mtxRwShaders. Array of statuses of running compilation threads (finished or not). */
        std::vector<std::future<bool>> vRunningCompilationThreads;

        /**
         * Use with @ref mtxRwShaders. Array of shader names marked to be removed from @ref compiledShaders
         * when they are no longer used.
         */
        std::vector<std::string> vShadersToBeRemoved;

        /**
         * Name of the file used to store global shader cache information.
         * Global shader cache information is used to determine if all shader cache is valid
         * or not (needs to be recompiled or not).
         */
        const std::string_view sGlobalShaderCacheParametersFileName = ".shader_cache.toml";

        /** Name of the section used in global shader cache information. */
        const std::string_view sGlobalShaderCacheParametersSectionName = "global_shader_cache_parameters";

        /** Name of the key for build mode, used in global shader cache information. */
        const std::string_view sGlobalShaderCacheParametersReleaseBuildKeyName = "is_release_build";

        /** Name of the key for used GPU, used in global shader cache information. */
        const std::string_view sGlobalShaderCacheParametersGpuKeyName = "gpu";

        /** Name of the file in which we store configurable values. */
        const std::string_view sConfigurationFileName = "shader_manager";

        /** Name of the key used in configuration file to store self validation interval. */
        const std::string_view sConfigurationSelfValidationIntervalKeyName = "self_validation_interval_min";

        /** Last time self validation was performed. */
        std::chrono::time_point<std::chrono::steady_clock> lastSelfValidationCheckTime;

        /** Amount of time after which to perform self validation. */
        long iSelfValidationIntervalInMin = 30; // NOLINT: don't do self validation too often

        /**
         * Atomic flag to set when destructor is called so that running compilation threads
         * are notified to finish early.
         */
        std::atomic_flag bIsShuttingDown;
    };
} // namespace ne
