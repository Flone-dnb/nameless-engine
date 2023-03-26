#include "misc/Globals.h"

// Custom.
#include "misc/Error.h"
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

// External.
#include "fmt/core.h"

namespace ne {
    std::string getApplicationName() {
#if defined(WIN32)

        TCHAR buffer[MAX_PATH] = {0};
        constexpr auto bufSize = static_cast<DWORD>(std::size(buffer));

        // Get the fully-qualified path of the executable.
        if (GetModuleFileName(nullptr, buffer, bufSize) == bufSize) {
            const Error err("failed to get path to the application, path is too long");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        return std::filesystem::path(buffer).stem().string();

#elif __linux__

        constexpr size_t bufSize = 1024; // NOLINT: should be more that enough
        char buf[bufSize] = {0};
        if (readlink("/proc/self/exe", &buf[0], bufSize) == -1) {
            const Error err("failed to get path to the application");
            err.showError();
            throw std::runtime_error(err.getError());
        }

        return std::filesystem::path(buf).stem().string();

#else
        static_assert(false, "not implemented");
#endif
    }

    std::filesystem::path getBaseDirectoryForConfigs() {
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

        const auto sHomePath = std::string(getenv("HOME"));
        if (sHomePath.empty()) {
            const Error err("Environment variable HOME is not set.");
            err.showError();
            throw std::runtime_error(err.getError());
        }

        basePath = fmt::format("{}/.config/", sHomePath);

#else
        static_assert(false, "not implemented");
#endif

        basePath /= sBaseEngineDirectoryName;

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directories(basePath);
        }

        return basePath;
    }

    std::string wstringToString(const std::wstring& sText) {
        std::string sOutput;

        sOutput.resize(sText.length());

#if defined(WIN32)
        size_t iResultBytes;
        wcstombs_s(&iResultBytes, sOutput.data(), sOutput.size() + 1, sText.c_str(), sText.size());
#elif __linux__
        wcstombs(&sOutput[0], sText.c_str(), sOutput.size());
#else
        static_assert(false, "not implemented");
#endif

        return sOutput;
    }

    std::wstring stringToWstring(const std::string& sText) {
        std::wstring sOutput;

        sOutput.resize(sText.length());

#if defined(WIN32)
        size_t iResultBytes;
        mbstowcs_s(&iResultBytes, sOutput.data(), sOutput.size() + 1, sText.c_str(), sText.size());
#elif __linux__
        mbstowcs(&sOutput[0], sText.c_str(), sOutput.size());
#else
        static_assert(false, "not implemented");
#endif

        return sOutput;
    }

    std::filesystem::path getPathToResDirectory() {
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
            throw std::runtime_error(err.getError());
        }

        pathToExecutable = std::filesystem::path(buf);

#else
        static_assert(false, "not implemented");
#endif

        // Construct path to the `res` directory.
        auto pathToRes = pathToExecutable.parent_path() / sResDirectoryName;
        if (!std::filesystem::exists(pathToRes)) {
            const Error err(fmt::format("`res` directory does not exist at \"{}\"", pathToRes.string()));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        return pathToRes;
    }

} // namespace ne
