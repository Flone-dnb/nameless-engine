#pragma once

// Standard.
#include <mutex>

// Custom.
#include "materials/Shader.h"

// External.
#include "ShaderIncluder.h"
#include "shaderc/shaderc.hpp"

namespace ne {
    class Renderer;

    /** Represents a compiled GLSL shader. */
    class GlslShader : public Shader {
    public:
        /**
         * Constructor. Used to create shader using cache.
         *
         * @param pRenderer            Used renderer.
         * @param pathToCompiledShader Path to compiled shader bytecode on disk.
         * @param sShaderName          Unique name of this shader.
         * @param shaderType           Type of this shader.
         */
        GlslShader(
            Renderer* pRenderer,
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType);

        GlslShader() = delete;
        GlslShader(const GlslShader&) = delete;
        GlslShader& operator=(const GlslShader&) = delete;

        virtual ~GlslShader() override = default;

        /**
         * Compiles a shader.
         *
         * @param pRenderer         Vulkan renderer.
         * @param cacheDirectory    Directory to store this shader's cache,
         * for example: ".../shader_cache/engine.default".
         * @param sConfiguration    Shader configuration text that will be added to the name.
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return One of the three values: compiled shader, string containing shader compilation
         * error/warning or an internal error.
         */
        static std::variant<std::shared_ptr<Shader>, std::string, Error> compileShader(
            Renderer* pRenderer,
            const std::filesystem::path& cacheDirectory,
            const std::string& sConfiguration,
            const ShaderDescription& shaderDescription);

        /**
         * Loads compiled SPIR-V bytecode from disk and stores it in memory.
         * Subsequent calls to this function will just return the bytecode pointer
         * (no disk loading will happen).
         *
         * @remark Returned pointer will only be valid while this object is alive.
         *
         * @return Error if something went wrong, otherwise a mutex to use while accessing the bytecode
         * and an array of bytes.
         */
        std::variant<std::pair<std::recursive_mutex, std::vector<char>>*, Error> getShaderSpirvBytecode();

        /**
         * Releases underlying shader data (bytecode, root signature, etc.) from memory (this object will not
         * be deleted) if the shader data was loaded into memory. Next time this shader will be needed the
         * data will be loaded from disk.
         *
         * @return `false` if was released from memory, `true` if was not loaded in memory previously.
         */
        virtual bool releaseShaderDataFromMemoryIfLoaded() override;

    protected:
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
        saveAdditionalCompilationResultsInfo(ConfigManager& cacheMetadataConfigManager) override;

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
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) override;

    private:
        /**
         * Converts shader type to a shader kind type used by `shaderc` library.
         *
         * @param shaderType Shader type to convert.
         *
         * @return Shader kind.
         */
        static shaderc_shader_kind convertShaderTypeToShadercShaderKind(ShaderType shaderType);

        /**
         * Converts error returned by ShaderIncluder to a string.
         *
         * @param error Error returned by ShaderIncluder.
         *
         * @return Text representation of the error.
         */
        static std::string convertShaderIncluderErrorToString(ShaderIncluder::Error error);

        /** SPIR-V bytecode (array of bytes) of the compiled shader. */
        std::pair<std::recursive_mutex, std::vector<char>> mtxSpirvBytecode;
    };
} // namespace ne
