#pragma once

// Standard.
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "shader/ShaderDescription.h"
#include "shader/general/format/VertexFormat.h"

namespace ne {
    class Renderer;
    class ConfigManager;

    /**
     * Base class for different types/formats of shaders to implement.
     *
     * Represents a single compiled shader variant from the ShaderPack.
     */
    class Shader {
    public:
        Shader() = delete;
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        virtual ~Shader() = default;

        /**
         * Returns name of the shader resource that stores frame-global constants.
         *
         * @return Shader resource name.
         */
        static constexpr const char* getFrameConstantsShaderResourceName() {
            return pFrameConstantBufferName;
        }

        /**
         * Returns the current amount shaders (vertex, pixel, etc.) loaded into the memory (RAM/VRAM).
         *
         * @return Current amount of shaders loaded into the memory (RAM/VRAM).
         */
        static size_t getCurrentAmountOfShadersInMemory();

        /**
         * Compiles a shader.
         *
         * @param pRenderer            Current renderer.
         * @param shaderCacheDirectory Directory to store this shader's cache,
         * for example: ".../shader_cache/engine.default".
         * @param sConfiguration       Shader configuration text that will be added to the name.
         * @param shaderDescription    Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return One of the three values: compiled shader, string containing shader compilation
         * error/warning or an internal error.
         */
        static std::variant<std::shared_ptr<Shader>, std::string, Error> compileShader(
            Renderer* pRenderer,
            const std::filesystem::path& shaderCacheDirectory,
            const std::string& sConfiguration,
            const ShaderDescription& shaderDescription);

        /**
         * Creates a new shader using shader cache.
         *
         * @param pRenderer                       Used renderer.
         * @param pathToCompiledShaderBytecode    Path to compiled shader bytecode on disk (with
         * configuration), for example: ".../shader_cache/engine.default.vs/shader16604691462768904089".
         * @param shaderDescription               Description that describes the shader and how the shader
         * should be compiled. Used for cache invalidation.
         * @param sShaderNameWithoutConfiguration Initial shader name without configuration hash,
         * this name is used for logging.
         * @param cacheInvalidationReason         Will be not empty if cache was invalidated
         * (i.e. cache can't be used).
         *
         * @return Error if the shader cache is corrupted/invalidated (cache invalidation reason
         * should have a value and returned Error will contain a full description of this invalidation
         * reason) or there was something wrong while attempting to load the cache,
         * otherwise a shader created using shader cache.
         */
        static std::variant<std::shared_ptr<Shader>, Error> createFromCache(
            Renderer* pRenderer,
            const std::filesystem::path& pathToCompiledShaderBytecode,
            ShaderDescription& shaderDescription,
            const std::string& sShaderNameWithoutConfiguration,
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason);

        /**
         * Returns unique name of this shader.
         *
         * @return Unique name of this shader.
         */
        std::string getShaderName() const;

        /**
         * Returns type of this shader.
         *
         * @return Shader type.
         */
        ShaderType getShaderType() const;

        /**
         * Returns vertex format used by this shader.
         *
         * @return Empty if not applicable (for example if this shader is a compute shader),
         * otherwise used vertex format.
         */
        std::optional<VertexFormat> getVertexFormat() const;

        /**
         * Releases underlying shader bytecode for each shader from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @return `false` if was released from memory, `true` if was not loaded in memory previously.
         */
        virtual bool releaseShaderDataFromMemoryIfLoaded() = 0;

    protected:
        /**
         * Constructor.
         *
         * @param pRenderer            Used renderer.
         * @param pathToCompiledShader Path to compiled shader bytecode on disk.
         * @param sShaderName          Unique name of this shader.
         * @param shaderType           Type of this shader.
         * @param vertexFormat         Vertex format that this shader uses (can be empty if not applicable,
         * for example if this is a compute shader).
         */
        Shader(
            Renderer* pRenderer,
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType,
            std::optional<VertexFormat> vertexFormat);

        /**
         * Derived shader classes should call this function once they load shader bytecode
         * into the memory from the disk.
         */
        static void notifyShaderBytecodeLoadedIntoMemory();

        /**
         * Derived shader classes should call this function once they release shader bytecode
         * from the memory.
         */
        static void notifyShaderBytecodeReleasedFromMemory();

        /**
         * Used to save data of shader language specific (additional) shader compilation results
         * (such as reflection data, i.e. if there are some other compilation results besides compiled
         * shader bytecode which is automatically hashed and checked)
         * to later check them in @ref checkCachedAdditionalCompilationResultsInfo.
         *
         * @param cacheMetadataConfigManager Config manager to write the data to.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        saveAdditionalCompilationResultsInfo(ConfigManager& cacheMetadataConfigManager) {
            return {};
        };

        /**
         * Used to check cached data of shader language specific (additional) shader compilation results
         * (such as reflection data, i.e. if there are some other compilation results besides compiled
         * shader bytecode which is automatically hashed and checked) whether its valid or not.
         *
         * @param cacheMetadataConfigManager Config manager to write the data to.
         * @param cacheInvalidationReason     Will be not empty if cache was invalidated
         * (i.e. cache can't be used).
         *
         * @return Error if some internal error occurred.
         */
        [[nodiscard]] virtual std::optional<Error> checkCachedAdditionalCompilationResultsInfo(
            ConfigManager& cacheMetadataConfigManager,
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
            return {};
        };

        /**
         * Returns path to compiled shader blob on disk.
         *
         * @return Error if the compiled shader does not exist, otherwise path to compiled shader.
         */
        std::variant<std::filesystem::path, Error> getPathToCompiledShader();

        /**
         * Returns renderer.
         *
         * @return renderer.
         */
        Renderer* getRenderer() const;

    private:
        /**
         * Compiles a HLSL/GLSL shader depending on the used renderer.
         *
         * @param pRenderer            Current renderer.
         * @param shaderCacheDirectory Directory to store this shader's cache,
         * for example: ".../shader_cache/engine.default".
         * @param sConfiguration       Shader configuration text that will be added to the name.
         * @param shaderDescription    Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return One of the three values: compiled shader, string containing shader compilation
         * error/warning or an internal error.
         */
        static std::variant<std::shared_ptr<Shader>, std::string, Error> compileRenderDependentShader(
            Renderer* pRenderer,
            const std::filesystem::path& shaderCacheDirectory,
            const std::string& sConfiguration,
            const ShaderDescription& shaderDescription);

        /**
         * Creates a new HLSL/GLSL shader depending on the used renderer, expects that all
         * cached shader data is valid.
         *
         * @param pRenderer                    Used renderer.
         * @param pathToSourceShaderFile       Path to shader source code file.
         * @param pathToCompiledShaderBytecode Path to compiled shader bytecode on disk.
         * @param sShaderName                  Unique name of this shader.
         * @param shaderType                   Type of this shader.
         * @param vertexFormat                 Can be empty if non applicable (for example if this shader is a
         * compute shader).
         *
         * @return Error if something went wrong, otherwise created shader.
         */
        static std::variant<std::shared_ptr<Shader>, Error> createRenderDependentShaderFromCache(
            Renderer* pRenderer,
            const std::filesystem::path& pathToSourceShaderFile,
            const std::filesystem::path& pathToCompiledShaderBytecode,
            const std::string& sShaderName,
            ShaderType shaderType,
            std::optional<VertexFormat> vertexFormat);

        /** Do not delete. Used renderer. */
        Renderer* const pRenderer = nullptr;

        /**
         * Vertex format that this shader uses.
         *
         * @remark Can be empty if non applicable (for example if this shader is a compute shader).
         */
        const std::optional<VertexFormat> optionalVertexFormat;

        /** Unique shader name received from ShaderManager. */
        const std::string sShaderName;

        /** Type of this shader. */
        const ShaderType shaderType;

        /** Path to compiled shader. */
        const std::filesystem::path pathToCompiledShader;

        /** Name of the key used to store compiled bytecode hash in the metadata file. */
        static inline const auto sCompiledBytecodeHashKeyName = "compiled_bytecode_hash";

        /** Name of the `cbuffer`/`uniform` resource used to store frame data in shaders. */
        static constexpr auto pFrameConstantBufferName = "frameData";
    };
} // namespace ne
