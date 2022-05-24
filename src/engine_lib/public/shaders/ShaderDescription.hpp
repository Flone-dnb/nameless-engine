#pragma once

// STL.
#include <filesystem>

// External.
#include "io/ConfigManager.h"

namespace ne {
    /**
     * Describes the type of a shader.
     *
     * @warning PLEASE, assign a value to each entry.
     */
    enum class ShaderType { VERTEX_SHADER = 0, PIXEL_SHADER = 1, COMPUTE_SHADER = 2 };

    /**
     * Describes a shader.
     */
    struct ShaderDescription {
        ShaderDescription() = default;

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
            const std::string& sShaderEntryFunctionName,
            const std::vector<std::string>& vDefinedShaderMacros) {
            this->sShaderName = sShaderName;
            this->pathToShaderFile = pathToShaderFile;
            this->shaderType = shaderType;
            this->sShaderEntryFunctionName = sShaderEntryFunctionName;
            this->vDefinedShaderMacros = vDefinedShaderMacros;
        }

        /**
         * Compares this shader description with other to see
         * if the serializable fields are equal.
         * This is usually done to check if shader cache is valid or not
         * (needs to be recompiled or not).
         *
         * @param other Other shader description to compare with.
         *
         * @return Whether the data is equal or not.
         */
        bool isSerializableDataEqual(const ShaderDescription& other) const {
            // Shader entry.
            if (sShaderEntryFunctionName != other.sShaderEntryFunctionName) {
                return false;
            }

            // Shader type.
            if (shaderType != other.shaderType) {
                return false;
            }

            // Shader macro defines.
            if (vDefinedShaderMacros.size() != other.vDefinedShaderMacros.size()) {
                return false;
            }
            if (!vDefinedShaderMacros.empty()) {
                for (const auto& macro : vDefinedShaderMacros) {
                    auto it = std::ranges::find(other.vDefinedShaderMacros, macro);
                    if (it == other.vDefinedShaderMacros.end()) {
                        return false;
                    }
                }
            }

            return true;
        }

        /**
         * Used to deserialize struct from .toml file.
         *
         * @param data Toml value.
         */
        void from_toml(const toml::value& data) { // NOLINT
            vDefinedShaderMacros = toml::find_or<std::vector<std::string>>(
                data, "defined_shader_macros", std::vector<std::string>{});
            sShaderEntryFunctionName = toml::find_or<std::string>(data, "shader_entry_function_name", "");
            shaderType = static_cast<ShaderType>(toml::find_or<int>(data, "shader_type", 0));
        }

        /**
         * Used to serialize struct to .toml file.
         *
         * @return Toml value.
         */
        toml::value into_toml() const // NOLINT
        {
            return toml::value{
                {"defined_shader_macros", vDefinedShaderMacros},
                {"shader_entry_function_name", sShaderEntryFunctionName},
                {"shader_type", static_cast<int>(shaderType)}};
        }

        // ----------------------------------------------------------------------------------------

        // ----------------------------------------
        // if adding new fields:
        // - add to constructor,
        // - if fields should be considered when validating cache,
        // add fields to @ref from_toml, @ref into_toml and @ref isSerializableDataEqual.
        // ----------------------------------------

        /** Array of defined macros for shader. */
        std::vector<std::string> vDefinedShaderMacros;
        /** Globally unique shader name. */
        std::string sShaderName;
        /** Path to the shader file. */
        std::filesystem::path pathToShaderFile;
        /** Type of the shader. */
        ShaderType shaderType;
        /** Name of the shader's entry function name. */
        std::string sShaderEntryFunctionName;

        // ----------------------------------------
        // if adding new fields:
        // - add to constructor,
        // - if fields should be considered when validating cache,
        // add fields to @ref from_toml, @ref into_toml and @ref isSerializableDataEqual.
        // ----------------------------------------
    };
} // namespace ne
