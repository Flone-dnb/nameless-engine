#include "Shader.h"

// Custom.
#include "misc/Error.h"
#include "render/Renderer.h"
#include "materials/ShaderFilesystemPaths.hpp"
#if defined(WIN32)
#include "hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#endif

// External.
#include "fmt/core.h"

/** Total amount of shader blobs loaded into the memory. */
static std::atomic<size_t> iTotalShaderInMemoryCount{0};

namespace ne {
    Shader::Shader(
        Renderer* pRenderer,
        std::filesystem::path pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType,
        const std::string& sSourceFileHash) {
        this->pathToCompiledShader = std::move(pathToCompiledShader);
        this->sShaderName = sShaderName;
        this->shaderType = shaderType;
        this->pUsedRenderer = pRenderer;
        this->sSourceFileHash = sSourceFileHash;
    }

    size_t Shader::getCurrentAmountOfShadersInMemory() { return iTotalShaderInMemoryCount.load(); }

    void Shader::notifyShaderBytecodeLoadedIntoMemory() { iTotalShaderInMemoryCount.fetch_add(1); }

    void Shader::notifyShaderBytecodeReleasedFromMemory() { iTotalShaderInMemoryCount.fetch_sub(1); }

    std::variant<std::shared_ptr<Shader>, std::string, Error> Shader::compileShader(
        Renderer* pRenderer,
        const std::filesystem::path& shaderCacheDirectory,
        const std::string& sConfiguration,
        const ShaderDescription& shaderDescription) {
        // Create shader directory if needed.
        if (!std::filesystem::exists(shaderCacheDirectory)) {
            std::filesystem::create_directories(shaderCacheDirectory);
        }

        // Compile shader.
        std::variant<std::shared_ptr<Shader>, std::string, Error> result;
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            result =
                HlslShader::compileShader(pRenderer, shaderCacheDirectory, sConfiguration, shaderDescription);
        }
#elif __linux__
        static_assert(false, "not implemented");
        // TODO:
//        if (dynamic_cast<VulkanRenderer*>(pRender)) {
//            pShader = std::make_shared<GlslShader>(
//                pRenderer, currentPathToCompiledShader, sCurrentShaderName, shaderType);
//        }
#else
        static_assert(false, "not implemented");
#endif

        if (std::holds_alternative<std::shared_ptr<Shader>>(result)) {
            // Success. Cache shader's description on disk.
            // Prepare path to the file.
            auto shaderCacheConfigurationPath =
                shaderCacheDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();
            shaderCacheConfigurationPath += sConfiguration;

            // Save to file.
            ConfigManager configManager;
            configManager.setValue<ShaderDescription>(
                "", ShaderDescription::getConfigurationFileSectionName(), shaderDescription);
            auto optionalError = configManager.saveFile(shaderCacheConfigurationPath, false);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addEntry();
                return error;
            }
        }

        return result;
    }

    std::variant<std::shared_ptr<Shader>, Error> Shader::createFromCache(
        Renderer* pRenderer,
        const std::filesystem::path& pathToCompiledShader,
        ShaderDescription& shaderDescription,
        const std::string& sShaderNameWithoutConfiguration,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        cacheInvalidationReason = {};
        // Make sure the specified path to compiled shader exists.
        if (!std::filesystem::exists(pathToCompiledShader)) {
            return Error(fmt::format(
                "the specified path to shader cache \"{}\" does not exist", pathToCompiledShader.string()));
        }

        // Prepare path to the file that stores metadata about this shader's cache.
        const auto shaderCacheConfigurationPath =
            pathToCompiledShader.string() + ConfigManager::getConfigFormatExtension();

        // Make sure the metadata file exists.
        if (!std::filesystem::exists(shaderCacheConfigurationPath)) [[unlikely]] {
            return Error(fmt::format(
                "cache metadata of the specified shader does not exist", shaderDescription.sShaderName));
        }

        // Read shader cache metadata file from disk.
        ConfigManager configManager;
        auto optionalError = configManager.loadFile(shaderCacheConfigurationPath);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        // Generate shader description that was specified when this shader was compiled.
        auto cachedShaderDescription = configManager.getValue<ShaderDescription>(
            "", ShaderDescription::getConfigurationFileSectionName(), ShaderDescription());

        // Check if the current shader description is equal to the shader description when
        // this shader was compiled.
        const auto reason = shaderDescription.isSerializableDataEqual(cachedShaderDescription);
        if (reason.has_value()) {
            // Something has changed, cache is no longer valid.
            Error err(fmt::format(
                "invalidated cache for shader \"{}\" (reason: {})",
                sShaderNameWithoutConfiguration,
                ShaderCacheInvalidationReasonDescription::getDescription(reason.value())));
            cacheInvalidationReason = reason.value();
            return err;
        }

        // Calculate source file hash so that we could determine what pixel/fragment/vertex shaders
        // were compiled from the same file.
        const auto sSourceFileHash = ShaderDescription::getShaderSourceFileHash(
            shaderDescription.pathToShaderFile, shaderDescription.sShaderName);
        if (sSourceFileHash.empty()) {
            return Error(fmt::format(
                "unable to calculate shader source file hash (shader path: \"{}\")",
                shaderDescription.pathToShaderFile.string()));
        }

        std::shared_ptr<Shader> pShader = nullptr;
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            pShader = std::make_shared<HlslShader>(
                pRenderer,
                pathToCompiledShader,
                shaderDescription.sShaderName,
                shaderDescription.shaderType,
                sSourceFileHash);
        }
#elif __linux__
        static_assert(false, "not implemented");
        // TODO:
//        if (dynamic_cast<VulkanRenderer*>(pRender)) {
//            pShader = std::make_shared<GlslShader>(
//                pRenderer, currentPathToCompiledShader, sCurrentShaderName, shaderType, sSourceFileHash);
//        }
#else
        static_assert(false, "not implemented");
#endif

        auto result = pShader->testIfShaderCacheIsCorrupted();
        if (result.has_value()) {
            auto err = result.value();
            err.addEntry();
            return err;
        }

        return pShader;
    }

    std::string Shader::getShaderName() const { return sShaderName; }

    std::variant<std::filesystem::path, Error> Shader::getPathToCompiledShader() {
        if (!std::filesystem::exists(pathToCompiledShader)) {
            const Error err(fmt::format(
                "path to compiled shader \"{}\" no longer exists", pathToCompiledShader.string()));
            return err;
        }
        return pathToCompiledShader;
    }

    Renderer* Shader::getUsedRenderer() const { return pUsedRenderer; }

    std::string Shader::getShaderSourceFileHash() const { return sSourceFileHash; }

    ShaderType Shader::getShaderType() const { return shaderType; }
} // namespace ne
