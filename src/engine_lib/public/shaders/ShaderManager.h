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

// Custom.
#include "misc/Error.h"
#include "shaders/ShaderDescription.hpp"

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
         * @param vShadersToCompile Array of shaders to compile.
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
         * Returns compiled shader (compiled using @ref compileShaders).
         *
         * @warning Do not delete returned pointer, shader manager keeps the ownership
         * of this pointer.
         *
         * @param sShaderName Name of this shader.
         *
         * @return Empty if the shader with the specified name was not found,
         * valid pointer otherwise.
         */
        std::optional<IShader*> getShader(const std::string& sShaderName);

    protected:
        /**
         * Compiles each shader. Usually called in another thread to do this work asynchronously.
         *
         * @param pPromiseFinish    Promise to set when finished or exited. Do not delete this pointer.
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
            std::promise<bool>* pPromiseFinish,
            std::vector<ShaderDescription> vShadersToCompile,
            const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
            const std::function<
                void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
            const std::function<void()>& onCompleted);

    private:
        /** Do not delete. Parent renderer that uses this shader manager. */
        IRenderer* pRenderer;

        /** Use for @ref shader and @ref vRunningCompilationThreads. */
        std::mutex mtxRwShaders;

        const std::array<char, 65> vValidCharactersForShaderName = {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
            'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
            'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
            'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '_', '-'};

        std::unordered_map<std::string, std::unique_ptr<IShader>> compiledShaders;
        std::vector<std::promise<bool>> vRunningCompilationThreads;

        const std::string_view sGlobalShaderCacheParametersFileName = ".shader_cache.toml";
        const std::string_view sGlobalShaderCacheParametersSectionName = "global shader cache parameters";
        const std::string_view sGlobalShaderCacheParametersReleaseBuildKeyName = "is_release_build";
        const std::string_view sGlobalShaderCacheParametersGpuKeyName = "gpu";

        std::atomic_flag bIsShuttingDown;
    };
} // namespace ne
