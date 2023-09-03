#include "misc/ProjectPaths.h"

// Standard.
#include <format>
#include <mutex>

// Custom.
#include "misc/Globals.h"
#include "misc/Error.h"

// OS.
#if defined(WIN32)
#include <ShlObj.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#elif __linux__
#include <unistd.h>
#include <sys/types.h>
#include <cstddef>
#include <cstdlib>
#endif

namespace ne {
    std::filesystem::path ProjectPaths::getPathToEngineConfigsDirectory() {
        return getPathToBaseConfigDirectory() / Globals::getApplicationName() / sEngineDirectoryName;
    }

    std::filesystem::path ProjectPaths::getPathToLogsDirectory() {
        return getPathToBaseConfigDirectory() / Globals::getApplicationName() / sLogsDirectoryName;
    }

    std::filesystem::path ProjectPaths::getPathToPlayerProgressDirectory() {
        return getPathToBaseConfigDirectory() / Globals::getApplicationName() / sProgressDirectoryName;
    }

    std::filesystem::path ProjectPaths::getPathToPlayerSettingsDirectory() {
        return getPathToBaseConfigDirectory() / Globals::getApplicationName() / sSettingsDirectoryName;
    }

    std::filesystem::path ProjectPaths::getPathToCompiledShadersDirectory() {
        return getPathToBaseConfigDirectory() / Globals::getApplicationName() / sShaderCacheDirectoryName;
    }

    std::filesystem::path ProjectPaths::getPathToResDirectory(ResourceDirectory directory) {
        std::filesystem::path path = getPathToResDirectory();

        switch (directory) {
        case ResourceDirectory::ROOT:
            return path;
        case ResourceDirectory::GAME: {
            path /= sGameResourcesDirectoryName;
            break;
        }
        case ResourceDirectory::ENGINE: {
            path /= sEngineResourcesDirectoryName;
            break;
        }
        case ResourceDirectory::EDITOR: {
            path /= sEditorResourcesDirectoryName;
            break;
        }
        };

        if (!std::filesystem::exists(path)) {
            const Error err(std::format("expected directory \"{}\" to exist", path.string()));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        return path;
    }

    std::filesystem::path ProjectPaths::getPathToBaseConfigDirectory() {
        std::filesystem::path basePath;

#if defined(WIN32)

        // Try to get AppData folder.
        PWSTR pPathTmp = nullptr;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pPathTmp);
        if (result != S_OK) {
            CoTaskMemFree(pPathTmp);

            const Error err(result);
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        basePath = pPathTmp;

        CoTaskMemFree(pPathTmp);

#elif __linux__

        static std::mutex mtxGetenv;
        {
            std::scoped_lock guard(mtxGetenv);

            const auto sHomePath = std::string(getenv("HOME")); // NOLINT: not thread safe
            if (sHomePath.empty()) {
                const Error err("environment variable HOME is not set");
                err.showError();
                throw std::runtime_error(err.getFullErrorMessage());
            }

            basePath = std::format("{}/.config/", sHomePath);
        }

#else
        static_assert(false, "not implemented");
#endif

        basePath /= Globals::getEngineDirectoryName();

        // Create path if not exists.
        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directories(basePath);
        }

        return basePath;
    }

    std::filesystem::path ProjectPaths::getPathToResDirectory() {
        std::filesystem::path pathToExecutable;

#if defined(WIN32)

        TCHAR buffer[MAX_PATH] = {0};
        constexpr auto bufSize = static_cast<DWORD>(std::size(buffer));

        // Get the fully-qualified path of the executable.
        if (GetModuleFileName(nullptr, buffer, bufSize) == bufSize) {
            const Error err("failed to get path to the application, path is too long");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        pathToExecutable = std::filesystem::path(buffer);

#elif __linux__

        constexpr size_t bufSize = 1024; // NOLINT: should be more that enough
        char buf[bufSize] = {0};
        if (readlink("/proc/self/exe", &buf[0], bufSize) == -1) {
            const Error err("failed to get path to the application");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        pathToExecutable = std::filesystem::path(buf);

#else
        static_assert(false, "not implemented");
#endif

        // Construct path to the resources directory.
        auto pathToRes = pathToExecutable.parent_path() / Globals::getResourcesDirectoryName();
        if (!std::filesystem::exists(pathToRes)) {
            const Error err(
                std::format("expected resources directory to exist at \"{}\"", pathToRes.string()));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        return pathToRes;
    }
} // namespace ne
