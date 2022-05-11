#include "IShader.h"

// STL.
#include <format>
#include <fstream>

// Custom.
#include "io/Logger.h"
#include "misc/Globals.h"
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

    std::filesystem::path IShader::getPathToShaderCacheDirectory() {
        // Check if current directory is writeable.
        namespace fs = std::filesystem;

        fs::path pathToShaderCacheDir(fs::current_path());

        const auto testFilePath = fs::current_path() / ".~~test~file~~";
        std::ofstream file(testFilePath);
        file.close();

        constexpr auto sShaderCacheDirName = "shader_cache";

        if (!fs::exists(testFilePath)) {
            // Use directory for configs.
            pathToShaderCacheDir = getBaseDirectoryForConfigs();
            pathToShaderCacheDir += getApplicationName();
            pathToShaderCacheDir /= sShaderCacheDirName;
            Logger::get().info(
                std::format(
                    "don't have permissions to write to the current directory "
                    "({}), using the config directory instead ({})",
                    fs::current_path().string(),
                    pathToShaderCacheDir.string()),
                "");
        } else {
            fs::remove(testFilePath);
            pathToShaderCacheDir /= sShaderCacheDirName;
        }

        if (!fs::exists(pathToShaderCacheDir)) {
            fs::create_directories(pathToShaderCacheDir);
        }

        return pathToShaderCacheDir;
    }
} // namespace ne
