#pragma once

// STL.
#include <filesystem>

// Custom.
#include "io/ConfigManager.h"
#include "io/Logger.h"

// External.
#include "xxHash/xxhash.h"

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
         * Used to deserialize struct from .toml file.
         *
         * @param data Toml value.
         */
        void from_toml(const toml::value& data) { // NOLINT
            vDefinedShaderMacros = toml::find_or<std::vector<std::string>>(
                data, "defined_shader_macros", std::vector<std::string>{});
            sShaderEntryFunctionName = toml::find_or<std::string>(data, "shader_entry_function_name", "");
            sSourceFileHash = toml::find_or<std::string>(data, "source_file_hash", "");
            shaderType = static_cast<ShaderType>(toml::find_or<int>(data, "shader_type", 0));
        }

        /**
         * Used to serialize struct to .toml file.
         *
         * @return Toml value.
         */
        toml::value into_toml() const { // NOLINT
            if (sSourceFileHash.empty()) {
                Logger::get().error(
                    std::format("shader source file hash is not calculated (shader: {})", sShaderName), "");
            }

            return toml::value{
                {"defined_shader_macros", vDefinedShaderMacros},
                {"shader_entry_function_name", sShaderEntryFunctionName},
                {"shader_type", static_cast<int>(shaderType)},
                {"source_file_hash", sSourceFileHash}};
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

    private:
        friend class IShader;

        /**
         * Calculates hash of shader source file.
         * Assumes that @ref pathToShaderFile exists.
         * This should be done before serialization.
         */
        void calculateShaderSourceFileHash() {
            if (pathToShaderFile.empty()) [[unlikely]] {
                Logger::get().error(
                    std::format("path to shader file is empty (shader: {})", sShaderName), "");
                return;
            }
            if (!std::filesystem::exists(pathToShaderFile)) [[unlikely]] {
                Logger::get().error(
                    std::format(
                        "shader file does not exist (shader: {}, path: {})",
                        sShaderName,
                        pathToShaderFile.string()),
                    "");
                return;
            }

            std::ifstream sourceFile(pathToShaderFile, std::ios::binary);

            sourceFile.seekg(0, std::ios::end);
            const long long iFileLength = sourceFile.tellg();
            sourceFile.seekg(0, std::ios::beg);

            std::vector<char> vFileData(iFileLength);

            sourceFile.read(vFileData.data(), iFileLength);
            sourceFile.close();

            sSourceFileHash = std::to_string(XXH3_64bits(vFileData.data(), iFileLength));
        }

        /**
         * Compares this shader description with other to see
         * if the serializable fields are equal.
         * This is usually done to check if shader cache is valid or not
         * (needs to be recompiled or not).
         *
         * @param other Other shader description to compare with.
         *
         * @return Whether the data is equal or not. If the data is not equal,
         * also has string that contains reason.
         */
        std::pair<bool, std::string> isSerializableDataEqual(const ShaderDescription& other) const {
            // Shader entry.
            if (sShaderEntryFunctionName != other.sShaderEntryFunctionName) {
                return std::make_pair(false, "shader entry function name changed");
            }

            // Shader type.
            if (shaderType != other.shaderType) {
                return std::make_pair(false, "shader type changed");
            }

            // Shader macro defines.
            if (vDefinedShaderMacros.size() != other.vDefinedShaderMacros.size()) {
                return std::make_pair(false, "defined shader macros changed");
            }
            if (!vDefinedShaderMacros.empty()) {
                for (const auto& macro : vDefinedShaderMacros) {
                    auto it = std::ranges::find(other.vDefinedShaderMacros, macro);
                    if (it == other.vDefinedShaderMacros.end()) {
                        return std::make_pair(false, "defined shader macros changed");
                    }
                }
            }

            // Compare source file hashes.
            if (sSourceFileHash != other.sSourceFileHash) {
                return std::make_pair(false, "shader source file changed");
            }

            return std::make_pair(true, "");
        }

        /** Shader source file hash, may be empty (not calculated yet). */
        std::string sSourceFileHash;
    };
} // namespace ne
