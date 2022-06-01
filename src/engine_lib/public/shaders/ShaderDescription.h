﻿#pragma once

// STL.
#include <filesystem>

// Custom.
#include "io/ConfigManager.h"

namespace ne {
    constexpr auto sShaderDescriptionLogCategory = "Shader Description";

    /**
     * Describes the type of a shader.
     */
    enum class ShaderType : int { VERTEX_SHADER = 0, PIXEL_SHADER = 1, COMPUTE_SHADER = 2 };

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
            const std::vector<std::string>& vDefinedShaderMacros);

        /**
         * Used to deserialize struct from .toml file.
         *
         * @param data Toml value.
         */
        void from_toml(const toml::value& data); // NOLINT

        /**
         * Used to serialize struct to .toml file.
         *
         * @return Toml value.
         */
        toml::value into_toml() const; // NOLINT

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
        ShaderType shaderType = ShaderType::VERTEX_SHADER;

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
         * Calculates hash of shader source file and returns it.
         * Assumes that @ref pathToShaderFile exists.
         *
         * @param pathToShaderSourceFile Path to shader source file.
         * @param sShaderName            Unique name of this shader (used for logging).
         *
         * @return Empty string if something went wrong, source file hash otherwise.
         */
        static std::string getShaderSourceFileHash(
            const std::filesystem::path& pathToShaderSourceFile, const std::string& sShaderName);

        /**
         * Uses @ref pathToShaderFile to recursively calculate
         * hashes of all include files to fill @ref shaderIncludeTree.
         */
        void calculateShaderIncludeTreeHashes();

        /**
         * Converts TOML table into shader include tree hash map.
         *
         * @param data TOML data formatted as table.
         *
         * @return Map [include chain (i.e. current shader), [relative include path, include file hash]].
         */
        static std::unordered_map<
            std::string /** include chain (i.e. current shader) */,
            std::
                unordered_map<std::string /** relative include path */, std::string /** include file hash */>>
        deserializeShaderIncludeTreeHashes(const toml::value& data);

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
        std::pair<bool, std::string> isSerializableDataEqual(ShaderDescription& other);

        /**
         * Scans shader file for "#include" entries and
         * recursively adds included files hashes.
         *
         * @param pathToShaderFile      Path to shader source file.
         * @param sCurrentIncludeChain  Current section (in TOML) text.
         * @param data                  TOML structure to write to.
         */
        static void serializeShaderIncludeTree(
            const std::filesystem::path& pathToShaderFile,
            std::string& sCurrentIncludeChain,
            toml::value& data);

        // ----------------------------------------------------------------------------------------

        /** Shader source file hash, may be empty (not calculated yet). */
        std::string sSourceFileHash;

        /**
         * Shader include tree hashes, contains relative include paths for each "#include" entry
         * in shader with included source file hash.
         * May be empty (not calculated yet).
         */
        std::unordered_map<
            std::string /** include chain (i.e. current shader) */,
            std::
                unordered_map<std::string /** relative include path */, std::string /** include file hash */>>
            shaderIncludeTreeHashes;

        /** Whether @ref shaderIncludeTreeHashes was initialized or not. */
        bool bIsShaderIncludeTreeHashesInitialized = false;

        /**
         * Used as initial text for include chain string (beginning
         * text in serialized form). After serialization
         * in TOML configuration it might look like this:
         * @code
         * "includes.default.post_process"
         *     ^------ [includes] initial text
         *              ^------ [default] shader from ShaderDescription (say shader A)
         *                        ^------ [post_process] shader B that shader A includes
         * @endcode
         */
        static constexpr std::string_view sInitialIncludeChainText = "includes";

        // ----------------------------------------
        // if adding new fields:
        // - if fields should be considered when validating cache,
        // add fields to @ref from_toml, @ref into_toml and @ref isSerializableDataEqual.
        // ----------------------------------------
    };
} // namespace ne