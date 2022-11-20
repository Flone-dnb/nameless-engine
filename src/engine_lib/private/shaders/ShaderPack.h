#pragma once

// Standard.
#include <mutex>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>

// Custom.
#include "shaders/ShaderDescription.h"
#include "shaders/ShaderParameter.h"

namespace ne {
    class Renderer;
    class Shader;

    /**
     * Represents a group of different variants of one shader
     * (typically this means one shader compiled with different combinations of predefined macros).
     */
    class ShaderPack {
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
         * Looks for a shader that matches the specified configuration and returns it.
         *
         * @warning If you are calling this function not the first time,
         * make sure you are not holding any references to the old shader
         * as we will try to release old shader's resources from memory.
         *
         * @param configuration New configuration.
         *
         * @return `nullptr` if the shader for this configuration was not found, otherwise
         * shader that matches this configuration.
         */
        std::shared_ptr<Shader> changeConfiguration(const std::set<ShaderParameter>& configuration);

        /**
         * Releases underlying shader bytecode for each shader from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @param bLogOnlyErrors Specify 'true' to only log errors, 'false' to log errors and info.
         * Specifying 'true' is useful when we are testing if shader cache is corrupted or not,
         * to make log slightly cleaner.
         *
         * @return 'false' if at least one shader variant was released from memory,
         * 'true' if all variants were not loaded into memory.
         */
        bool releaseShaderPackDataFromMemoryIfLoaded(bool bLogOnlyErrors = false);

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
         */
        ShaderPack(const std::string& sShaderName);

        /** Initial shader name (without configuration text). */
        std::string sShaderName;

        /** Mutex for working with shaders. */
        std::mutex mtxShaders;

        /** A shader that was requested in the last call to @ref changeConfiguration. */
        std::shared_ptr<Shader>* pPreviouslyRequestedShader = nullptr;

        /** Map of shaders in this pack. */
        std::unordered_map<std::set<ShaderParameter>, std::shared_ptr<Shader>, ShaderParameterSetHash>
            shaders;
    };
} // namespace ne
