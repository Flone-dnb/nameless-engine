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
            const std::vector<ShaderDescription>& vShadersToCompile,
            const std::function<void(size_t iCompiledShaderCount, size_t iTotalShadersToCompile)>& onProgress,
            const std::function<
                void(ShaderDescription shaderDescription, std::variant<std::string, Error> error)>& onError,
            const std::function<void()>& onCompleted);

    private:
        /** Do not delete. Parent renderer that uses this shader manager. */
        IRenderer* pRenderer;

        /** Use for @ref shader and @ref vRunningCompilationThreads. */
        std::mutex mtxRwShaders;

        std::unordered_map<std::string, std::unique_ptr<IShader>> shaders;
        std::vector<std::promise<bool>> vRunningCompilationThreads;

        std::atomic_flag bIsShuttingDown;
    };
} // namespace ne
