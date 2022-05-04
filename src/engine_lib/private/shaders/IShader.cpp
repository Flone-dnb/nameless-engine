#include "IShader.h"

// STL.
#include <format>

// Custom.
#include "misc/Error.h"

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

    std::filesystem::path IShader::getPathToCompiledShader() {
        if (!std::filesystem::exists(pathToCompiledShader)) {
            const Error err(
                std::format("path to compiled shader {} no longer exists", pathToCompiledShader.string()));
            err.showError();
            throw std::runtime_error(err.getError());
        }
        return pathToCompiledShader;
    }
} // namespace ne
