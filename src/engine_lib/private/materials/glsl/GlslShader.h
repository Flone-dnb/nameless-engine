#pragma once

// Standard.
#include <mutex>
#include <optional>
#include <unordered_map>

// Custom.
#include "materials/Shader.h"
#include "materials/glsl/DescriptorSetLayoutGenerator.h"

// External.
#include "ShaderIncluder.h"
#include "shaderc/shaderc.hpp"
#include "vulkan/vulkan.h"

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
         * @param sShaderEntryFunctionName Name of the entry function.
         */
        GlslShader(
            Renderer* pRenderer,
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType,
            const std::string& sShaderEntryFunctionName);

        GlslShader() = delete;
        GlslShader(const GlslShader&) = delete;
        GlslShader& operator=(const GlslShader&) = delete;

        virtual ~GlslShader() override = default;

        /**
         * Returns vertex description for vertex input binding.
         *
         * @return Vertex input binding description.
         */
        static VkVertexInputBindingDescription getVertexBindingDescription();

        /**
         * Returns description of all vertex attributes.
         *
         * @return Vertex attribute descriptions.
         */
        static std::array<VkVertexInputAttributeDescription, 3> getVertexAttributeDescriptions();

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
        std::variant<std::pair<std::recursive_mutex, std::vector<char>>*, Error> getCompiledBytecode();

        /**
         * Returns information about descriptor layout that can be used for this shader.
         *
         * @return Empty if descriptor layout information was not collected yet, use @ref getCompiledBytecode
         * to collect and load everything, otherwise descriptor layout info.
         */
        std::pair<std::mutex, std::optional<std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo>>>*
        getDescriptorSetLayoutBindingInfo();

        /**
         * Returns name of the shader's entry function.
         *
         * @return Entry function name.
         */
        std::string getShaderEntryFunctionName() const;

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

        /**
         * Loads shader data (bytecode, descriptor set layout info, etc.) from disk cache
         * if it's not loaded yet.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> loadShaderDataFromDiskIfNotLoaded();

        /** SPIR-V bytecode (array of bytes) of the compiled shader. */
        std::pair<std::recursive_mutex, std::vector<char>> mtxSpirvBytecode;

        /**
         * Contains information used to descriptor set layout.
         *
         * @remark Might not be calculated yet, see @ref loadShaderDataFromDiskIfNotLoaded for collecting root
         * signature information.
         */
        std::pair<std::mutex, std::optional<std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo>>>
            mtxDescriptorSetLayoutBindingInfo;

        /** Name of the entry function of this shader. */
        std::string sShaderEntryFunctionName;

        /** Index of the vertex input binding. */
        static constexpr uint32_t iVertexBindingIndex = 0;

        /** Name of the section used to store descriptor set layout info. */
        static inline const auto sDescriptorSetLayoutSectionName = "Descriptor Set Layout";
    };
} // namespace ne
