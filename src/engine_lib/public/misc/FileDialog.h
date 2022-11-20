#pragma once

// Standard.
#include <filesystem>
#include <vector>
#include <string>
#include <optional>

namespace ne {
    /** Various file dialog windows: open file, save file, select directory and etc. */
    class FileDialog {
    public:
        /**
         * Shows "Open File" dialog window to allow the user to select file(s).
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

        /**
         * Shows "Save File" dialog window to ask the user where to save a file.
         *
         * Example:
         * @code
         * // 3 versions that provide equal result:
         * const auto optional = ne::FileDialog::saveFile("Save Configuration", {"Configuration", "*.toml"});
         * const auto optional = ne::FileDialog::saveFile("Save Configuration", {"Configuration", ".toml"});
         * const auto optional = ne::FileDialog::saveFile("Save Configuration", {"Configuration", "toml"});
         * // Possible result on Windows when the user saves a file with the name "123":
         * "D:\Downloads\123.toml".
         * @endcode
         *
         * @param sTitle      Title of the dialog window.
         * @param fileType    A pair of "Name of the file type" and "file extension".
         * @param directory   Starting directory to show.
         *
         * @return Empty if the user clicked "Cancel", otherwise path to the file to
         * save (with added extension if the file type was specified).
         */
        static std::optional<std::filesystem::path> saveFile(
            const std::string& sTitle,
            const std::pair<std::string, std::string>& fileType = {"All Files", {"*"}},
            const std::filesystem::path& directory = std::filesystem::current_path());

        /**
         * Shows "Select Directory" dialog window to ask the user to select a directory.
         *
         * @param sTitle      Title of the dialog window.
         * @param directory   Starting directory to show.
         *
         * @return Empty if the user clicked "Cancel", otherwise path to the selected directory.
         */
        static std::optional<std::filesystem::path> selectDirectory(
            const std::string& sTitle,
            const std::filesystem::path& directory = std::filesystem::current_path());
    };
} // namespace ne
