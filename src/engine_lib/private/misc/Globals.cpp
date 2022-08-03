#include "Globals.h"

// Custom.
#include "Error.h"

#if defined(WIN32)

#include <ShlObj.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#elif __linux__

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#endif

namespace ne {
    std::string getApplicationName() {
#if defined(WIN32)

        TCHAR buffer[MAX_PATH] = {0};
        constexpr auto bufSize = static_cast<DWORD>(std::size(buffer));

        // Get the fully-qualified path of the executable.
        if (GetModuleFileName(nullptr, buffer, bufSize) == bufSize) {
            const Error err("failed to get path to the application, path is too long");
            err.showError();
            throw std::runtime_error(err.getError());
        }

        return std::filesystem::path(buffer).stem().string();

#elif __linux__

        constexpr size_t bufSize = 512; // NOLINT
        char buf[bufSize] = {0};
        if (readlink("/proc/self/exe", &buf, bufSize) == -1) {
            const Error err("failed to get path to the application");
            err.showError();
            throw std::runtime_error(err.getError());
        }

        static_assert(false, "check if this part actually works");

        return std::filesystem::path(buffer).stem().string();

#endif
    }

    std::filesystem::path getBaseDirectoryForConfigs() {
        std::filesystem::path basePath;

#if defined(WIN32)

        // Try to get AppData folder.
        PWSTR pathTmp;
        const HRESULT resultPath = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pathTmp);
        if (resultPath != S_OK) {
            CoTaskMemFree(pathTmp);

            const Error err("failed to initialize base logger directory");
            err.showError();
            throw std::runtime_error(err.getError());
        }

        basePath = pathTmp;

        CoTaskMemFree(pathTmp);

#elif __linux__

        struct passwd* pw = getpwuid_r(getuid());
        basePath = std::format("{}/.config/", pw->pw_dir);
        static_assert(false, "check if this part actually works");

#endif

        basePath /= sBaseEngineDirectoryName;

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directories(basePath);
        }

        return basePath;
    }

    std::string wstringToString(const std::wstring& sText) {
        std::string sOutput;
        size_t iResultBytes;

        sOutput.resize(sText.length());

        wcstombs_s(&iResultBytes, &sOutput[0], sOutput.size() + 1, sText.c_str(), sText.size());

        return sOutput;
    }

    std::wstring stringToWstring(const std::string& sText) {
        std::wstring sOutput;
        size_t iResultBytes;

        sOutput.resize(sText.length());

        mbstowcs_s(&iResultBytes, &sOutput[0], sOutput.size() + 1, sText.c_str(), sText.size());

        return sOutput;
    }
} // namespace ne
