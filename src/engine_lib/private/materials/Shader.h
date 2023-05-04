#pragma once

// Standard.
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "materials/ShaderDescription.h"

namespace ne {
    class Renderer;

    /**
     * Base class for different types/formats of shaders to implement.
     */
    class Shader {
    public:
        Shader() = delete;
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        virtual ~Shader() = default;

        /**
         * Returns the current amount shaders (vertex, pixel, etc.) loaded into the memory (RAM/VRAM).
         *
         * @return Current amount of shaders loaded into the memory (RAM/VRAM).
         */
        static size_t getCurrentAmountOfShadersInMemory();

        /**
         * Tests if the shader cache for this shader is corrupted or not.
         *
         * @remark This function should be used before you want to use the shader cache.
         *
         * @return Error if shader cache is corrupted.
         */
        [[nodiscard]] virtual std::optional<Error> testIfShaderCacheIsCorrupted() = 0;

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
            std::shared_ptr<Shader> /** Compiled shader pack. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(
            Renderer* pRenderer,
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
        static std::variant<std::shared_ptr<Shader>, Error> createFromCache(
            Renderer* pRenderer,
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
         * Returns a hash that was calculated from the shader source file that was used to
         * compile this shader.
         *
         * @return Shader source file hash.
         */
        std::string getShaderSourceFileHash() const;

        /**
         * Releases underlying shader bytecode for each shader from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @param bLogOnlyErrors Specify 'true' to only log errors, 'false' to log errors and info.
         * Specifying 'true' is useful when we are testing if shader cache is corrupted or not,
         * to make log slightly cleaner.
         *
         * @return `false` if was released from memory, `true` if was not loaded in memory previously.
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
         * @param sSourceFileHash      Shader source file hash, used to tell what shaders were compiled from
         * the same file.
         */
        Shader(
            Renderer* pRenderer,
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType,
            const std::string& sSourceFileHash);

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
        Renderer* getUsedRenderer() const;

    private:
        /** Do not delete. Used renderer. */
        Renderer* pUsedRenderer = nullptr;

        /** Unique shader name received from ShaderManager. */
        std::string sShaderName;

        /** Type of this shader. */
        ShaderType shaderType;

        /** Path to compiled shader. */
        std::filesystem::path pathToCompiledShader;

        /** Shader source file hash, used to tell what shaders were compiled from the same file. */
        std::string sSourceFileHash;

        /** Name of the category used for logging. */
        static inline const auto sShaderLogCategory = "Shader";
    };
} // namespace ne
