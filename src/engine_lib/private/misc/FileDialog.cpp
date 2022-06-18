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
} // namespace ne

#if defined(WIN32)
#pragma pop_macro("IGNORE")
#pragma pop_macro("MessageBox")
#endif