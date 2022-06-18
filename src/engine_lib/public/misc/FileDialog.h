#pragma once

// STL.
#include <filesystem>
#include <vector>
#include <string>

namespace ne {
    /** Various file dialog windows: open file, save file, select directory and etc. */
    class FileDialog {
    public:
        /**
         * Shows "Open File" dialog window.
         *
         * Example:
         * @code
         * const auto vSelected = ne::FileDialog::openFile(
         *     "Select audio and image files",
         *     {
         *       {"Audio Files", "*.mp3 *.wav *.flac *.ogg"},
         *       {"Image Files", "*.png *.jpg *.jpeg *.bmp"}
         *     },
         *     std::filesystem::current_path(),
         *     true);
         * @endcode
         *
         * @param sTitle                        Title of the dialog window.
         * @param vFileTypeFilters              Filter that determines which types of files can be selected.
         * Each pair contains filter name and space separated types.
         * @param directory                     Starting directory to show.
         * @param bAllowSelectingMultipleFiles  Whether to allow selecting multiple files or not.
         *
         * @return Array of selected files (if bAllowSelectingMultipleFiles is 'true'), one selected file
         * (if bAllowSelectingMultipleFiles is 'false') and empty array if the used clicked "Cancel".
         */
        static std::vector<std::filesystem::path> openFile(
            const std::string& sTitle,
            const std::vector<std::pair<std::string, std::string>>& vFileTypeFilters = {{"All Files", {"*"}}},
            const std::filesystem::path& directory = std::filesystem::current_path(),
            bool bAllowSelectingMultipleFiles = false);
    };
} // namespace ne
