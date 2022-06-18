#include "IShader.h"

// STL.
#include <format>

// Custom.
#include "misc/Globals.h"
#include "misc/Error.h"
#include "render/IRenderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shaders/hlsl/HlslShader.h"
#endif

namespace ne {
    IShader::IShader(
        std::filesystem::path pathToCompiledShader, const std::string& sShaderName, ShaderType shaderType) {
        if (!std::filesystem::exists(pathToCompiledShader)) {
            const Error err(
                std::format("the specified path {} does not exist", pathToCompiledShader.string()));
            err.showError();
            throw std::runtime_error(err.getError());
        }
        this->pathToCompiledShader = std::move(pathToCompiledShader);
        this->sShaderName = sShaderName;
        this->shaderType = shaderType;
    }

    std::variant<std::shared_ptr<IShader>, std::string, Error>
    IShader::compileShader(ShaderDescription& shaderDescription, IRenderer* pRenderer) {
        std::optional<ShaderCacheInvalidationReason> unused;
        return compileShader(shaderDescription, pRenderer, unused);
    }

    std::variant<std::shared_ptr<IShader>, std::string, Error> IShader::compileShader(
        ShaderDescription& shaderDescription,
        IRenderer* pRenderer,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer)) {
            return compileShader<HlslShader>(shaderDescription, cacheInvalidationReason);
        }
#endif

        const auto err = Error("no shader type is associated with the "
                               "current renderer (not implemented)");
        err.showError();
        throw std::runtime_error(err.getError());
    }

    std::string IShader::getShaderName() const { return sShaderName; }

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

    ShaderType IShader::getShaderType() const { return shaderType; }
} // namespace ne
