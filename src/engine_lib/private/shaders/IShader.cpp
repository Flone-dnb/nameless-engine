#include "IShader.h"

// Custom.
#include "misc/Error.h"
#include "render/IRenderer.h"
#include "io/Logger.h"
#include "shaders/ShaderFilesystemPaths.hpp"
#if defined(WIN32)
#include "hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#endif

// External.
#include "fmt/core.h"

namespace ne {
    IShader::IShader(
        IRenderer* pRenderer,
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

    std::variant<std::shared_ptr<IShader>, std::string, Error> IShader::compileShader(
        IRenderer* pRenderer,
        const std::filesystem::path& shaderCacheDirectory,
        const std::string& sConfiguration,
        const ShaderDescription& shaderDescription) {
        // Create shader directory if needed.
        if (!std::filesystem::exists(shaderCacheDirectory)) {
            std::filesystem::create_directories(shaderCacheDirectory);
        }

        // Compile shader.
        std::variant<std::shared_ptr<IShader>, std::string, Error> result;
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer)) {
            result =
                HlslShader::compileShader(pRenderer, shaderCacheDirectory, sConfiguration, shaderDescription);
        }
#else
        Error err("no renderer for this platform");
        err.showError();
        throw std::runtime_error(err.getError());
        // TODO:
//        if (dynamic_cast<VulkanRenderer*>(pRender)) {
//            pShader = std::make_shared<GlslShader>(
//                pRenderer, currentPathToCompiledShader, sCurrentShaderName, shaderType);
//        }
#endif

        if (std::holds_alternative<std::shared_ptr<IShader>>(result)) {
            auto shaderCacheConfigurationPath =
                shaderCacheDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();
            shaderCacheConfigurationPath += sConfiguration;

            // Success. Cache the configuration on disk.
            ConfigManager configManager;
            configManager.setValue<ShaderDescription>(
                "", ShaderDescription::getConfigurationFileSectionName(), shaderDescription);
            configManager.saveFile(shaderCacheConfigurationPath, false);
        }
        return result;
    }

    std::variant<std::shared_ptr<IShader>, Error> IShader::createFromCache(
        IRenderer* pRenderer,
        const std::filesystem::path& pathToCompiledShader,
        ShaderDescription& shaderDescription,
        const std::string& sShaderNameWithoutConfiguration,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        cacheInvalidationReason = {};
        // Create shader directory if needed.
        if (!std::filesystem::exists(pathToCompiledShader)) {
            return Error("shader cache does not exist");
        }

        const auto shaderCacheConfigurationPath =
            pathToCompiledShader.string() + ConfigManager::getConfigFormatExtension();

        ConfigManager configManager;

        // Check if cached config exists.
        if (std::filesystem::exists(shaderCacheConfigurationPath)) {
            // Check if cache is valid.
            configManager.loadFile(shaderCacheConfigurationPath);
            auto cachedShaderDescription = configManager.getValue<ShaderDescription>(
                "", ShaderDescription::getConfigurationFileSectionName(), ShaderDescription());

            auto reason = shaderDescription.isSerializableDataEqual(cachedShaderDescription);
            if (reason.has_value()) {
                Error err(fmt::format(
                    "invalidated cache for shader \"{}\" (reason: {})",
                    sShaderNameWithoutConfiguration,
                    ShaderCacheInvalidationReasonDescription::getDescription(reason.value())));
                cacheInvalidationReason = reason.value();
                return err;
            }
        }

        // Calculate source file hash.
        const auto sSourceFileHash = ShaderDescription::getShaderSourceFileHash(
            shaderDescription.pathToShaderFile, shaderDescription.sShaderName);
        if (sSourceFileHash.empty()) {
            return Error(fmt::format(
                "unable to calculate shader source file hash (shader path: \"{}\")",
                shaderDescription.pathToShaderFile.string()));
        }

        std::shared_ptr<IShader> pShader;
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer)) {
            pShader = std::make_shared<HlslShader>(
                pRenderer,
                pathToCompiledShader,
                shaderDescription.sShaderName,
                shaderDescription.shaderType,
                sSourceFileHash);
        }
#else
        const auto err = Error("no shader type is associated with the "
                               "current renderer (not implemented)");
        err.showError();
        throw std::runtime_error(err.getError());
        // TODO:
//        if (dynamic_cast<VulkanRenderer*>(pRender)) {
//            pShader = std::make_shared<GlslShader>(
//                pRenderer, currentPathToCompiledShader, sCurrentShaderName, shaderType, sSourceFileHash);
//        }
#endif

        auto result = pShader->testIfShaderCacheIsCorrupted();
        if (result.has_value()) {
            auto err = result.value();
            err.addEntry();
            return err;
        }

        return pShader;
    }

    std::string IShader::getShaderName() const { return sShaderName; }

    std::variant<std::filesystem::path, Error> IShader::getPathToCompiledShader() {
        if (!std::filesystem::exists(pathToCompiledShader)) {
            const Error err(fmt::format(
                "path to compiled shader \"{}\" no longer exists", pathToCompiledShader.string()));
            return err;
        }
        return pathToCompiledShader;
    }

    IRenderer* IShader::getUsedRenderer() const { return pUsedRenderer; }

    std::string IShader::getShaderSourceFileHash() const { return sSourceFileHash; }

    ShaderType IShader::getShaderType() const { return shaderType; }
} // namespace ne
