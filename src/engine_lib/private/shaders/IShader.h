#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "shaders/ShaderDescription.h"

namespace ne {
    class IRenderer;
    /**
     * Interface class for different types/formats of shaders.
     */
    class IShader {
    public:
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
         * @return Error if shader cache is corrupted and was deleted.
         */
        virtual std::optional<Error> testIfShaderCacheIsCorrupted() = 0;

        /**
         * Compiles a shader.
         *
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         * @param pRenderer Used renderer.
         *
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::shared_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(ShaderDescription& shaderDescription, IRenderer* pRenderer);

        /**
         * Compiles a shader.
         *
         * This function is another version of @ref compileShader that has additional parameter
         * of cache invalidation reason. Used for testing purposes.
         *
         * @param shaderDescription       Description that describes the shader and how the shader should be
         * compiled.
         * @param pRenderer               Used renderer.
         * @param cacheInvalidationReason If shader cache was invalidated will contain invalidation reason.
         *
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::shared_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(
            ShaderDescription& shaderDescription,
            IRenderer* pRenderer,
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason);

        /**
         * Compiles a shader.
         *
         * This function is another version of @ref compileShader that has additional parameter
         * of cache invalidation reason and templated argument. Used for testing purposes.
         *
         * @param shaderDescription       Description that describes the shader and how the shader should be
         * compiled.
         * @param pRenderer               Used renderer.
         * @param cacheInvalidationReason If shader cache was invalidated will contain invalidation reason.
         *
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        template <typename ShaderType>
        requires std::derived_from<ShaderType, IShader>
        static std::variant<
            std::shared_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(
            ShaderDescription& shaderDescription,
            IRenderer* pRenderer,
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason);

        /**
         * Returns unique name of this shader.
         *
         * @return Unique name of this shader.
         */
        std::string getShaderName() const;

        /**
         * Returns path to the directory used to store shader cache.
         *
         * @return Path to shader cache directory.
         */
        static std::filesystem::path getPathToShaderCacheDirectory();

        /**
         * Returns type of this shader.
         *
         * @return Shader type.
         */
        ShaderType getShaderType() const;

        /**
         * Releases underlying shader bytecode from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @param bLogOnlyErrors Specify 'true' to only log errors, 'false' to log errors and info.
         * Specifying 'true' is useful when we are testing if shader cache is corrupted or not,
         * to make log slightly cleaner.
         *
         * @return 'false' if was released from memory, 'true' if not loaded into memory.
         */
        virtual bool releaseShaderDataFromMemoryIfLoaded(bool bLogOnlyErrors = false) = 0;

    protected:
        /**
         * Returns path to compiled shader blob on disk.
         *
         * @return Path to compiled shader.
         */
        std::filesystem::path getPathToCompiledShader();

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

        /** Directory name to store compiled shaders. */
        static constexpr std::string_view sShaderCacheDirectoryName = "shader_cache";
    };

    template <typename ShaderType>
    requires std::derived_from<ShaderType, IShader> std::variant<std::shared_ptr<IShader>, std::string, Error>
    IShader::compileShader(
        ShaderDescription& shaderDescription,
        IRenderer* pRenderer,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        cacheInvalidationReason = {};

        const auto shaderCacheFilePath = getPathToShaderCacheDirectory() / shaderDescription.sShaderName;
        const auto shaderCacheConfigurationPath =
            shaderCacheFilePath.string() + ConfigManager::getConfigFormatExtension();
        ConfigManager configManager;

        bool bUseCache = false;

        // Check if cached config exists.
        if (std::filesystem::exists(shaderCacheConfigurationPath)) {
            // See if we need to recompile or use cache.
            configManager.loadFile(shaderCacheFilePath);
            auto cachedShaderDescription = configManager.getValue<ShaderDescription>(
                "", ShaderDescription::getConfigurationFileSectionName(), ShaderDescription());

            auto reason = shaderDescription.isSerializableDataEqual(cachedShaderDescription);
            if (!reason.has_value()) {
                Logger::get().info(
                    std::format("found valid cache for shader \"{}\"", shaderDescription.sShaderName), "");
                bUseCache = true;
            } else {
                Logger::get().info(
                    std::format(
                        "invalidated cache for shader \"{}\" (reason: {})",
                        shaderDescription.sShaderName,
                        ShaderCacheInvalidationReasonDescription::getDescription(reason.value())),
                    "");
                cacheInvalidationReason = reason.value();
            }
        }

        if (bUseCache) {
            auto pShader = std::make_shared<ShaderType>(
                pRenderer, shaderCacheFilePath, shaderDescription.sShaderName, shaderDescription.shaderType);

            std::optional<Error> optionalError = pShader->testIfShaderCacheIsCorrupted();
            if (optionalError.has_value()) {
                Logger::get().error(
                    std::format(
                        "shader \"{}\" cache files are corrupted, removing corrupted cache file",
                        shaderDescription.sShaderName),
                    "");

                // It's enough to delete shader cache configuration file, everything else will be overwritten
                // next time.
                std::filesystem::remove(shaderCacheConfigurationPath);

                optionalError->addEntry();
                return optionalError.value();
            }
            return pShader;
        } else {
            auto result = ShaderType::compileShader(pRenderer, std::move(shaderDescription));
            if (std::holds_alternative<std::shared_ptr<IShader>>(result)) {
                // Success. Cache configuration.
                configManager.setValue<ShaderDescription>(
                    "", ShaderDescription::getConfigurationFileSectionName(), shaderDescription);
                configManager.saveFile(shaderCacheFilePath, false);
            }
            return result;
        }
    }
} // namespace ne
