#pragma once

// STL.
#include <filesystem>

namespace ne {
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
            const std::string& sShaderName,
            const std::filesystem::path& pathToShaderFile,
            ShaderType shaderType,
            const std::wstring& sShaderEntryFunctionName,
            const std::vector<std::wstring>& vDefinedShaderMacros) {
            this->sShaderName = sShaderName;
            this->pathToShaderFile = pathToShaderFile;
            this->shaderType = shaderType;
            this->sShaderEntryFunctionName = sShaderEntryFunctionName;
            this->vDefinedShaderMacros = vDefinedShaderMacros;
        }

        /** Array of defined macros for shader. */
        std::vector<std::wstring> vDefinedShaderMacros;
        /** Globally unique shader name. */
        std::string sShaderName;
        /** Path to the shader file. */
        std::filesystem::path pathToShaderFile;
        /** Type of the shader. */
        ShaderType shaderType;
        /** Name of the shader's entry function name. */
        std::wstring sShaderEntryFunctionName;
    };
} // namespace ne
