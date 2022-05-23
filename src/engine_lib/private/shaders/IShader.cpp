#include "IShader.h"

// STL.
#include <format>

// Custom.
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

    std::variant<std::unique_ptr<IShader>, std::string, Error>
    IShader::compileShader(const ShaderDescription& shaderDescription, IRenderer* pRenderer) {
        if (dynamic_cast<DirectXRenderer*>(pRenderer)) {
            auto result = HlslShader::compileShader(std::move(shaderDescription));
            if (std::holds_alternative<std::unique_ptr<IShader>>(result)) {
                // Success. Create shader cache parameters.
                const auto shaderCacheDir = getPathToShaderCacheDirectory() / shaderDescription.sShaderName;
                // TODO
            }
            return result;
        } else {
            const auto err = Error("no shader type is associated with the "
                                   "current renderer (not implemented)");
            err.showError();
            throw std::runtime_error(err.getError());
        }
    }

    std::filesystem::path IShader::getPathToCompiledShader() {
        if (!std::filesystem::exists(pathToCompiledShader)) {
            const Error err(
                std::format("path to compiled shader {} no longer exists", pathToCompiledShader.string()));
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
