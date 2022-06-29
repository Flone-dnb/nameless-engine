#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "shaders/ShaderDescription.h"
#include "shaders/ShaderPack.h"

namespace ne {
    class IRenderer;
    /**
     * Interface class for different types/formats of shaders.
     */
    class IShader {
    public:
        IShader() = delete;
        IShader(const IShader&) = delete;
        IShader& operator=(const IShader&) = delete;

        virtual ~IShader() = default;

        /**
         * Tests if shader cache for this shader is corrupted or not
         * and deletes the cache if it's corrupted.
         *
         * @remark This function should be used if you want to use shader cache.
         *
         * @return Error if shader cache is corrupted.
         */
        virtual std::optional<Error> testIfShaderCacheIsCorrupted() = 0;

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
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::shared_ptr<IShader> /** Compiled shader pack. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(
            IRenderer* pRenderer,
            const std::filesystem::path& shaderCacheDirectory,
            const std::string& sConfiguration,
            const ShaderDescription& shaderDescription);

        /**
         * Creates a new shader using shader cache.
         *
         * @param pRenderer            Used renderer.
         * @param pathToCompiledShader Path to compiled shader bytecode on disk (with configuration),
         * for example: ".../shader_cache/engine.default.vs/shader16604691462768904089".
         * @param shaderDescription    Description that describes the shader and how the shader should be
         * compiled. Used for cache invalidation.
         * @param sShaderNameWithoutConfiguration Initial shader name without configuration hash,
         * this name is used for logging.
         * @param cacheInvalidationReason Will be not empty if cache was invalidated.
         *
         * @return Returns error if the shader cache is corrupted or was invalidated,
         * otherwise a shader created using shader cache.
         */
        static std::variant<std::shared_ptr<IShader>, Error> createFromCache(
            IRenderer* pRenderer,
            const std::filesystem::path& pathToCompiledShader,
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
        virtual bool releaseShaderDataFromMemoryIfLoaded(bool bLogOnlyErrors = false) = 0;

    protected:
        /**
         * Constructor.
         *
         * @param pRenderer            Used renderer.
         * @param pathToCompiledShader Path to compiled shader blob on disk.
         * @param sShaderName          Unique name of this shader.
         * @param shaderType           Type of this shader.
         */
        IShader(
            IRenderer* pRenderer,
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType);

        /**
         * Returns path to compiled shader blob on disk.
         *
         * @return Error if the compiled shader does not exist, otherwise path to compiled shader.
         */
        std::variant<std::filesystem::path, Error> getPathToCompiledShader();

        /**
         * Returns used renderer.
         *
         * @return Used renderer.
         */
        IRenderer* getUsedRenderer() const;

    private:
        /** Unique shader name received from ShaderManager. */
        std::string sShaderName;

        /** Type of this shader. */
        ShaderType shaderType;

        /** Path to compiled shader. */
        std::filesystem::path pathToCompiledShader;

        /** Do not delete. Used renderer. */
        IRenderer* pUsedRenderer = nullptr;
    };
} // namespace ne
