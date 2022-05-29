#include "IShader.h"

// STL.
#include <format>

// Custom.
#include "io/ConfigManager.h"
#include "io/Logger.h"
#include "misc/Globals.h"
#include "misc/Error.h"
#include "render/IRenderer.h"
#include "render/directx/DirectXRenderer.h"

namespace ne {
    IShader::IShader(std::filesystem::path pathToCompiledShader) {
        if (!std::filesystem::exists(pathToCompiledShader)) {
            const Error err(
                std::format("the specified path {} does not exist", pathToCompiledShader.string()));
            err.showError();
            throw std::runtime_error(err.getError());
        }
        this->pathToCompiledShader = std::move(pathToCompiledShader);
    }

    std::variant<std::shared_ptr<IShader>, std::string, Error>
    IShader::compileShader(ShaderDescription& shaderDescription, IRenderer* pRenderer) {
        const auto shaderCacheFilePath = getPathToShaderCacheDirectory() / shaderDescription.sShaderName;
        ConfigManager configManager;

        bool bUseCache = false;

        // Check if cached config exists.
        if (std::filesystem::exists(
                shaderCacheFilePath.string() + ConfigManager::getConfigFormatExtension())) {
            // See if we need to recompile or use cache.
            configManager.loadFile(shaderCacheFilePath);
            auto cachedShaderDescription =
                configManager.getValue<ShaderDescription>("", "shader_description", ShaderDescription());

            auto [bIsEqual, sReason] = shaderDescription.isSerializableDataEqual(cachedShaderDescription);
            if (bIsEqual) {
                Logger::get().info(
                    std::format("found valid cache for shader \"{}\"", shaderDescription.sShaderName), "");
                bUseCache = true;
            } else {
                Logger::get().info(
                    std::format(
                        "invalidated cache for shader \"{}\" (reason: {})",
                        shaderDescription.sShaderName,
                        sReason),
                    "");
            }
        }

        if (dynamic_cast<DirectXRenderer*>(pRenderer)) {
            if (bUseCache) {
                return std::make_unique<HlslShader>(shaderCacheFilePath);
            } else {
                auto result = HlslShader::compileShader(std::move(shaderDescription));
                if (std::holds_alternative<std::shared_ptr<IShader>>(result)) {
                    // Success. Cache configuration.
                    configManager.setValue<ShaderDescription>("", "shader_description", shaderDescription);
                    configManager.saveFile(shaderCacheFilePath, false);
                }
                return result;
            }
        } else {
            const auto err = Error("no shader type is associated with the "
                                   "current renderer (not implemented)");
            err.showError();
            throw std::runtime_error(err.getError());
        }
    }

    std::filesystem::path IShader::getPathToCompiledShader() {
        if (!std::filesystem::exists(pathToCompiledShader)) {
            const Error err(std::format(
                "path to compiled shader \"{}\" no longer exists", pathToCompiledShader.string()));
            err.showError();
            throw std::runtime_error(err.getError());
        }
        return pathToCompiledShader;
    }

    std::filesystem::path IShader::getPathToShaderCacheDirectory() {
        std::filesystem::path basePath = getBaseDirectoryForConfigs();
        basePath += getApplicationName();

        basePath /= sShaderCacheDirectoryName;

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directories(basePath);
        }
        return basePath;
    }
} // namespace ne
