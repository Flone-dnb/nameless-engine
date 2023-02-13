#pragma once

// Standard.
#include <mutex>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>

// Custom.
#include "materials/ShaderDescription.h"
#include "materials/ShaderParameter.h"

namespace ne {
    class Renderer;
    class Shader;

    /**
     * Represents a group of different variants of one shader
     * (typically this means one shader compiled with different combinations of predefined macros).
     */
    class ShaderPack {
        // Shader manager can change shader pack configuration.
        friend class ShaderManager;

    public:
        ShaderPack() = delete;
        ShaderPack(const ShaderPack&) = delete;
        ShaderPack& operator=(const ShaderPack&) = delete;

        virtual ~ShaderPack() = default;

        /**
         * Compiles a shader pack.
         *
         * @param pRenderer         Used renderer.
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return Returns one of the three values:
         * @arg compiled shader pack
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::shared_ptr<ShaderPack> /** Compiled shader pack. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShaderPack(Renderer* pRenderer, const ShaderDescription& shaderDescription);

        /**
         * Creates a new shader pack using shader cache.
         *
         * @param pRenderer             Used renderer.
         * @param shaderDescription     Description that describes the shader and how the shader should be
         * compiled. Used for cache invalidation.
         * @param cacheInvalidationReason Will be not empty if cache was invalidated. Used for testing.
         *
         * @return Returns error if shader cache is corrupted or was invalidated,
         * otherwise a shader pack created using cache.
         */
        static std::variant<std::shared_ptr<ShaderPack>, Error> createFromCache(
            Renderer* pRenderer,
            const ShaderDescription& shaderDescription,
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason);

        /**
         * Releases underlying shader bytecode for each shader from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @param bLogOnlyErrors Specify `true` to only log errors, `false` to log errors and info.
         * Specifying `true` is useful when we are testing if shader cache is corrupted or not,
         * to make log slightly cleaner.
         *
         * @return `false` if at least one shader variant was released from memory,
         * `true` if all variants were not loaded into memory.
         */
        bool releaseShaderPackDataFromMemoryIfLoaded(bool bLogOnlyErrors = false);

        /**
         * Returns a shader that matches the current shader configuration.
         *
         * @return Shader.
         */
        std::shared_ptr<Shader> getShader();

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
        ShaderType getShaderType();

    private:
        /**
         * Constructor to create an empty shader pack.
         *
         * @param sShaderName Initial name of the shader.
         * @param shaderType  Type of shaders this pack stores.
         */
        ShaderPack(const std::string& sShaderName, ShaderType shaderType);

        /**
         * Looks for a shader that matches the specified configuration and saves it to be returned
         * in @ref getShader.
         *
         * @warning If the configuration is changed we will try to release
         * old shader's resources from the memory.
         * Make sure no object is holding shared pointers to old shader (old configuration),
         * otherwise there would be an error printed in the logs.
         *
         * @param configuration New configuration.
         *
         * @return `true` if the shader for this configuration was not found, otherwise `false`.
         */
        [[nodiscard]] bool setConfiguration(const std::set<ShaderParameter>& configuration);

        /** Last configuration that was set in @ref setConfiguration. */
        std::set<ShaderParameter> configuration;

        /** Initial shader name (without configuration text). */
        std::string sShaderName;

        /** Type of shaders this pack stores. */
        ShaderType shaderType;

        /**
         * Shader that matched the requested configuration in the last call to @ref setConfiguration.
         * Must be used with mutex.
         */
        std::pair<std::mutex, std::shared_ptr<Shader>*> mtxCurrentConfigurationShader;

        /**
         * Map of shaders in this pack.
         * Must be used with the mutex.
         */
        std::pair<
            std::mutex,
            std::unordered_map<std::set<ShaderParameter>, std::shared_ptr<Shader>, ShaderParameterSetHash>>
            mtxShadersInPack;

        /** Name of the category used for logging. */
        static inline const auto sShaderPackLogCategory = "Shader Pack";
    };
} // namespace ne
