#pragma once

// Standard.
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <atomic>
#include <variant>
#include <array>
#include <chrono>

// Custom.
#include "misc/Error.h"
#include "material/ShaderDescription.h"
#include "material/ShaderPack.h"

namespace ne {
    class Shader;
    class Renderer;

    /** Handles shader compilation and controls shader registry. */
    class ShaderManager {
        // Only ShaderUser should be able to work with shaders.
        friend class ShaderUser;

        // Renderer sets shaders' configuration.
        friend class Renderer;

    public:
        /**
         * Constructor.
         *
         * @param pRenderer Parent renderer that uses this shader manager.
         */
        ShaderManager(Renderer* pRenderer);

        ShaderManager() = delete;
        ShaderManager(const ShaderManager&) = delete;
        ShaderManager& operator=(const ShaderManager&) = delete;

        virtual ~ShaderManager() = default;

        /**
         * Add shaders to be asynchronously compiled.
         *
         * Compiled shaders are stored on disk, when a shader is needed it will be
         * automatically loaded from disk into memory and when no longer being used it will be
         * released from memory (stored on disk again).
         *
         * @remark If a shader was already compiled previously compilation results will be stored in
         * the shader cache on the disk and if the cache is currently valid
         * (for ex. shader source file has not changed, included shader source files also not changed,
         * defined shader macros not changed, build configuration not changed and etc.),
         * shader compilation will be skipped and instead the shader will be loaded from the cache
         * (which is faster than compiling the shader).
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
         * not be available, you will need to fix the error and add this shader again).
         * @param onCompleted       Callback function that will be called once all shaders are compiled.
         *
         * @remark Note that all callback functions will be queued to be executed on the main thread and
         * will be called later from the main thread before next frame is rendered.
         * Because callbacks are called from the main thread it's safe to call functions that are
         * marked as "should only be called from the main thread" from the callback functions.
         * If you are using member functions as callbacks you need to make sure that the owner object
         * of these member functions will not be deleted until onCompleted is called.
         *
         *
         * @return An error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> compileShaders(
            std::vector<ShaderDescription> vShadersToCompile,
            const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
            const std::function<
                void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
            const std::function<void()>& onCompleted);

        /**
         * Checks if the shader name is free (no shader with this name found) to be used
         * in @ref compileShaders.
         *
         * @param sShaderName Name to check.
         *
         * @return `true` if can be used (no shader with this name found), `false` otherwise.
         */
        bool isShaderNameCanBeUsed(const std::string& sShaderName);

        /**
         * Removes the shader if nobody is referencing it, otherwise marks shader to be removed
         * later.
         *
         * Typically you would not use this function as we expect you to make
         * one call to @ref compileShaders in the beginning of the game to compile
         * ALL of your shaders (for all levels) and never remove them as compiled shaders
         * are not stored in memory, they are stored on disk and when actually needed/used
         * loaded from disk to memory. If some shader was used but no longer needed it will
         * be released from memory until someone will need it again.
         *
         * If somebody is still referencing this shader, the shader will be added to
         * "to remove" array and will be removed later when nobody
         * is referencing this shader (specifically when only one
         * std::shared_ptr<Shader> instance pointing to this shader
         * will exist (it will exist in @ref ShaderManager as @ref ShaderManager
         * stores pointer to each shader)).
         *
         * @param sShaderName Shader name to be marked for removal.
         *
         * @return 'true' if someone is still referencing this shader
         * and it cannot be removed right now, thus shader's name still
         * cannot be used in @ref compileShaders. Returns 'false' if
         * nobody was referencing this shader and it was removed, thus
         * shader's name can now be used in @ref compileShaders.
         */
        bool markShaderToBeRemoved(const std::string& sShaderName);

        /**
         * Analyzes the current state to see if any shader-related errors have place (like unused
         * shaders in memory or etc.). Fixes errors and reports them in log.
         */
        void performSelfValidation();

    protected:
        /**
         * Sets renderer's shader configuration for specific types of shaders.
         *
         * @warning If the configuration is changed we will try to release
         * old shader resources from the memory.
         * Make sure no object is holding shared pointers to old shaders (with old configuration),
         * otherwise there would be an error printed in the logs.
         *
         * @param configuration Configuration to set.
         * @param shaderType    Types of shaders to set the configuration to.
         */
        void
        setRendererConfigurationForShaders(const std::set<ShaderMacro>& configuration, ShaderType shaderType);

        /**
         * Compiles each shader. Executed as a thread pooled task to do this work asynchronously.
         *
         * @param iQueryId          Unique number used to differentiate different calls @ref compileShaders.
         * @param pCompiledShaderCount Current total number of shaders compiled (in query).
         * @param iTotalShaderCount Total number of shaders to compile in this query
         * (might be bigger than the size of the vShadersToCompile argument because the query is
         * divided in smaller tasks).
         * @param shaderDescription Shader to compile.
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
        void compileShaderTask(
            size_t iQueryId,
            const std::shared_ptr<std::atomic<size_t>>& pCompiledShaderCount,
            size_t iTotalShaderCount,
            ShaderDescription shaderDescription,
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
        std::optional<std::shared_ptr<ShaderPack>> getShader(const std::string& sShaderName);

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
         * Looks if any of the global shader cache parameters changed
         * (such as build mode, shader model, etc.), clears shader cache
         * directory and creates a fresh new shader cache directory with up to date info.
         *
         * @remark If no global shader cache metadata file existed it will create it.
         *
         * @return An error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> refreshShaderCache();

    private:
        /** Internal shader data. */
        struct ShaderData {
            /**
             * Map of compiled (added) shaders.
             *
             * Storing shared pointer for each pack here instead of just reading from disk, creating
             * a new shared pointer, giving shared pointer and forgetting about it to make sure
             * there's not going to be multiple copies of one shader.
             */
            std::unordered_map<std::string, std::shared_ptr<ShaderPack>> compiledShaders;

            /**
             * Array of shader names marked to be removed from @ref compiledShaders when they
             * are no longer used.
             */
            std::vector<std::string> vShadersToBeRemoved;
        };

        /** Do not delete. Parent renderer that uses this shader manager. */
        Renderer* pRenderer;

        /** Internal shader data guarded by mutex. */
        std::pair<std::recursive_mutex, ShaderData> mtxShaderData;

        /**
         * Total number of "compile shaders" queries.
         * Used to differentiate calls to @ref compileShaderTask.
         */
        std::atomic<size_t> iTotalCompileShadersQueries = 0;

        /**
         * Name of the file used to store global shader cache information.
         * Global shader cache information is used to determine if all shader cache is valid
         * or not (needs to be recompiled or not).
         *
         * Starts with a dot on purpose (because no shader can start with a dot - reserved for internal use).
         */
        static inline const std::string_view sGlobalShaderCacheParametersFileName = ".shader_cache.toml";

        /** Name of the key for build mode, used in global shader cache information. */
        static inline const std::string_view sGlobalShaderCacheReleaseBuildKeyName = "is_release_build";

        /** Name of the key for vertex shader model, used in global shader cache information. */
        static inline const std::string_view sGlobalShaderCacheHlslVsModelKeyName = "hlsl_vs";

        /** Name of the key for pixel shader model, used in global shader cache information. */
        static inline const std::string_view sGlobalShaderCacheHlslPsModelKeyName = "hlsl_ps";

        /** Name of the key for compute shader model, used in global shader cache information. */
        static inline const std::string_view sGlobalShaderCacheHlslCsModelKeyName = "hlsl_cs";

        /** Name of the key for renderer's type, used in global shader cache information. */
        static inline const std::string_view sGlobalShaderCacheRendererTypeKeyName = "renderer_type";

        /**
         * Array of characters that can be used for shader name.
         * We limit amount of valid characters because we store compiled shaders on disk
         * and different filesystems have different limitations for file names.
         */
        static inline const std::array<char, 65> vValidCharactersForShaderName = {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
            'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
            'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
            'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '_', '-'};

        /** Maximum length of a shader name. */
        static inline const size_t iMaximumShaderNameLength = 40;
    };
} // namespace ne
