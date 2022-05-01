#pragma once

// STL.
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>

// Custom.
#include "misc/Error.h"

namespace ne {
    class Shader;

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
         */
        ShaderDescription(
            const std::string &sShaderName,
            const std::filesystem::path &pathToShaderFile,
            ShaderType shaderType,
            const std::wstring &sShaderEntryFunctionName) {
            this->sShaderName = sShaderName;
            this->pathToShaderFile = pathToShaderFile;
            this->shaderType = shaderType;
            this->sShaderEntryFunctionName = sShaderEntryFunctionName;
        }

    private:
        std::string sShaderName;
        std::filesystem::path pathToShaderFile;
        ShaderType shaderType;
        std::wstring sShaderEntryFunctionName;
    };

    class ShaderManager {
    public:
        ShaderManager() = default;

        ShaderManager(const ShaderManager &) = delete;
        ShaderManager &operator=(const ShaderManager &) = delete;

        virtual ~ShaderManager() = default;

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
        std::unordered_map<std::string, std::unique_ptr<Shader>> shaders;
        std::mutex mtxRwShaders;
    };
} // namespace ne
