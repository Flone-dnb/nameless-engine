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

namespace ne {
    class IShader;

    /**
     * Describes the type of a shader.
     */
    enum class ShaderType { VERTEX_SHADER, PIXEL_SHADER, COMPUTE_SHADER };

    /**
     * Describes a shader.
     */
    struct ShaderDescription {
        /**
         * Constructor.
         *
         * @param sShaderName              Globally unique shader name.
         * @param pathToShaderFile         Path to the shader file.
         * @param shaderType               Type of the shader.
         * @param sShaderEntryFunctionName Name of the shader's entry function name.
         * For example: if the shader type is vertex shader, then this value should
         * contain name of the function used for vertex processing (from shader's file, "VS" for
         * example).
         * @param vDefinedShaderMacros     Array of defined macros for shader.
         */
        ShaderDescription(
            const std::string &sShaderName,
            const std::filesystem::path &pathToShaderFile,
            ShaderType shaderType,
            const std::wstring &sShaderEntryFunctionName,
            const std::vector<std::string> &vDefinedShaderMacros) {
            this->sShaderName = sShaderName;
            this->pathToShaderFile = pathToShaderFile;
            this->shaderType = shaderType;
            this->sShaderEntryFunctionName = sShaderEntryFunctionName;
            this->vDefinedShaderMacros = vDefinedShaderMacros;
        }

        /** Array of defined macros for shader. */
        std::vector<std::string> vDefinedShaderMacros;
        /** Globally unique shader name. */
        std::string sShaderName;
        /** Path to the shader file. */
        std::filesystem::path pathToShaderFile;
        /** Type of the shader. */
        ShaderType shaderType;
        /** Name of the shader's entry function name. */
        std::wstring sShaderEntryFunctionName;
    };

    /**
     * Controls shader compilation, shader registry.
     */
    class ShaderManager {
    public:
        ShaderManager() = default;

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
        std::unordered_map<std::string, std::unique_ptr<IShader>> shaders;
        std::vector<std::promise<bool>> vRunningCompilationThreads;
        std::mutex mtxRwShaders;
        std::atomic_flag bIsShuttingDown;
    };
} // namespace ne
