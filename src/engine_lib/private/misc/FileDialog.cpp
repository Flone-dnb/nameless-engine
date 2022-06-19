#include "misc/FileDialog.h"

// External.
#include "portable-file-dialogs.h"
#if defined(WIN32)
#pragma push_macro("MessageBox")
#undef MessageBox
#pragma push_macro("IGNORE")
#undef IGNORE
#endif

namespace ne {
    std::vector<std::filesystem::path> FileDialog::openFile(
        const std::string& sTitle,
        const std::vector<std::pair<std::string, std::string>>& vFileTypeFilters,
        const std::filesystem::path& directory,
        bool bAllowSelectingMultipleFiles) {
        // Convert filter to the format pfd expects.
        std::vector<std::string> vFilters;
        for (const auto& [sName, sTypeFilters] : vFileTypeFilters) {
            vFilters.push_back(std::format("{} ({})", sName, sTypeFilters));
            vFilters.push_back(sTypeFilters);
        }

        const auto vResult = pfd::open_file(
                                 sTitle,
                                 directory.string(),
                                 vFilters,
                                 bAllowSelectingMultipleFiles ? pfd::opt::multiselect : pfd::opt::none)
                                 .result();
        // Convert result.
        std::vector<std::filesystem::path> vSelectedPaths;
        for (const auto& sPath : vResult) {
            vSelectedPaths.push_back(sPath);
        }

        return vSelectedPaths;
    }

    std::optional<std::filesystem::path> FileDialog::saveFile(
        const std::string& sTitle,
        const std::pair<std::string, std::string>& fileType,
        const std::filesystem::path& directory) {
        std::string sFileExtension = fileType.second;

        // Convert filter to the format pfd expects.
        std::vector<std::string> vFilters;
        if (!fileType.second.starts_with("*")) {
            if (!fileType.second.starts_with(".")) {
                sFileExtension.insert(0, ".");
            }
            sFileExtension.insert(0, "*");
        }

        vFilters.push_back(std::format("{} ({})", fileType.first, sFileExtension));
        vFilters.push_back(sFileExtension);

        auto sResult = pfd::save_file(sTitle, directory.string(), vFilters, pfd::opt::none).result();

        if (sResult.empty()) {
            return {};
        } else {
            if (sFileExtension.size() > 1) {
                const auto sExtension = sFileExtension.substr(1); // skip "*".
                if (!sResult.ends_with(sExtension)) {
                    sResult += sExtension;
                }
            }
            return sResult;
        }
    }

    std::optional<std::filesystem::path>
    FileDialog::selectDirectory(const std::string& sTitle, const std::filesystem::path& directory) {
        const auto sResult = pfd::select_folder(sTitle, directory.string(), pfd::opt::none).result();

        if (sResult.empty()) {
            return {};
        } else {
            return sResult;
        }
    }
} // namespace ne

#if defined(WIN32)
#pragma pop_macro("IGNORE")
#pragma pop_macro("MessageBox")
#endif