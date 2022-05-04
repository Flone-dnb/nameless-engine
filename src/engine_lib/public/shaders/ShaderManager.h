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

// Custom.
#include "misc/Error.h"
#include "shaders/ShaderDescription.hpp"

namespace ne {
    constexpr auto sShaderManagerLogCategory = "ShaderManager";

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
        ShaderManager(IRenderer *pRenderer);

        ShaderManager() = delete;
        ShaderManager(const ShaderManager &) = delete;
        ShaderManager &operator=(const ShaderManager &) = delete;

        virtual ~ShaderManager();

        /**
         * Add shaders to be asynchronously compiled.
         *
         * @param vShadersToCompile Array of shaders to compile.
         * @param onProgress        Callback function that will be called when each shader is compiled.
         * This will also be called when all shaders are compiled (together with 'onCompleted').
         * The first argument is number of compiled shaders and the second one is total number of
         * shaders to compile.
         * @param onCompleted       Callback function that will be called once all shaders are compiled.
         *
         * @return An error if something went wrong.
         */
        std::optional<Error> compileShaders(
            const std::vector<ShaderDescription> &vShadersToCompile,
            std::function<void(
                size_t /** Number of compiled shaders */, size_t /** Total number of shaders to compile */)>
                onProgress,
            std::function<void()> onCompleted);

    private:
        /** Do not delete. Parent renderer that uses this shader manager. */
        IRenderer *pRenderer;

        std::unordered_map<std::string, std::unique_ptr<IShader>> shaders;
        std::vector<std::promise<bool>> vRunningCompilationThreads;
        std::mutex mtxRwShaders;
        std::atomic_flag bIsShuttingDown;
    };
} // namespace ne
