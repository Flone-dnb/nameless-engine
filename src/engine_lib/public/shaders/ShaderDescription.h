#pragma once

// STL.
#include <filesystem>

// Custom.
#include "io/ConfigManager.h"

namespace ne {
    /**
     * Describes the type of a shader.
     */
    enum class ShaderType : int { VERTEX_SHADER = 0, PIXEL_SHADER = 1, COMPUTE_SHADER = 2 };

    /**
     * Describes different reasons for shader cache invalidation.
     *
     * @note Add new entries to @ref ShaderCacheInvalidationReasonDescription.
     */
    enum class ShaderCacheInvalidationReason {
        ENTRY_FUNCTION_NAME_CHANGED,
        SHADER_TYPE_CHANGED,
        DEFINED_SHADER_MACROS_CHANGED,
        SHADER_SOURCE_FILE_CHANGED,
        SHADER_INCLUDE_TREE_CONTENT_CHANGED,
    };

    /** Maps ShaderCacheInvalidationReason to text description. */
    struct ShaderCacheInvalidationReasonDescription {
        /** Map between ShaderCacheInvalidationReason and text description. */
        inline static const std::unordered_map<ShaderCacheInvalidationReason, const char*>
            cacheInvalidationReasons = {
                {ShaderCacheInvalidationReason::ENTRY_FUNCTION_NAME_CHANGED,
                 "shader entry function name changed"},
                {ShaderCacheInvalidationReason::SHADER_TYPE_CHANGED, "shader type changed"},
                {ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED,
                 "defined shader macros changed"},
                {ShaderCacheInvalidationReason::SHADER_SOURCE_FILE_CHANGED, "shader source file changed"},
                {ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED,
                 "shader include tree content changed"}};
        /**
         * Returns description in form of text for the specified reason.
         *
         * @param reason Shader cache invalidation reason.
         *
         * @return Reason description.
         */
        static const char* getDescription(ShaderCacheInvalidationReason reason) {
            const auto it = cacheInvalidationReasons.find(reason);
            if (it == cacheInvalidationReasons.end()) {
                throw std::runtime_error("no description is provided for this reason");
            }
            return it->second;
        }
    };

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
         * Copy constructor.
         *
         * @param other Other object.
         */
        ShaderDescription(const ShaderDescription& other) noexcept = default;

        /**
         * Copy assignment.
         *
         * @param other other object.
         *
         * @return Result of copy assignment.
         */
        ShaderDescription& operator=(const ShaderDescription& other) noexcept = default;

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShaderDescription(ShaderDescription&& other) noexcept = default;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShaderDescription& operator=(ShaderDescription&& other) noexcept = default;

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

        // ------------------------------- ! if adding new fields ! ------------------------------------
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

        // ------------------------------- ! if adding new fields ! ------------------------------------
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
         * hashes of all include files to fill @ref shaderIncludeTreeHashes.
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
         * has a value that contains a reason.
         */
        std::optional<ShaderCacheInvalidationReason> isSerializableDataEqual(ShaderDescription& other);

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

        // ------------------------------- ! if adding new fields ! ------------------------------------
        // if adding new fields:
        // - if fields should be considered when validating cache,
        // add fields to @ref from_toml, @ref into_toml and @ref isSerializableDataEqual.
        // ----------------------------------------

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
        static inline std::string_view sInitialIncludeChainText = "includes";

        /** Name of the category used for logging. */
        inline static const char* sShaderDescriptionLogCategory = "Shader Description";
    };
} // namespace ne
