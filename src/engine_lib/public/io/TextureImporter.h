#pragma once

// Standard.
#include <optional>
#include <filesystem>

// Custom.
#include "misc/Error.h"
#include "material/TextureFilteringPreference.h"

namespace ne {
    enum class TextureImportFormat {
        R,          //< BC4 compression, only one 8 bit channel, can be used for heightmaps for example.
        RG,         //< BC5 compression, 8 bits for R and 8 bits for G channel, can be used for normal maps.
        RGB,        //< BC1 compression, bits per channel: 5 for R, 6 for G, 5 for B.
        RGB_1BIT_A, //< BC1 compression, bits per channel: 5 for R, 6 for G, 5 for B and 0 or 1 bit for alpha.
        RGB_8BIT_A, //< BC3 compression, bits per channel: 5 for R, 6 for G, 5 for B and 8 bit for alpha.
        HDR,        //< BC6H compression, used for HDR textures.
        RGB_HIGH_QUALITY,  //< BC7 compression, high quality of the compressed image but bigger file size and
                           // longer import.
        RGBA_HIGH_QUALITY, //< BC7 compression, high quality of the compressed image but bigger file size and
                           // longer import.
    };

    /**
     * Provides static functions for importing files in special formats (such as PNG) as textures
     * into engine formats.
     */
    class TextureImporter {
    public:
        TextureImporter() = delete;

        /**
         * Returns file name that importer adds before extension to the imported texture file.
         *
         * @return File name.
         */
        static constexpr std::string_view getImportedFileName() { return sImportedFileName; }

        /**
         * Returns the name of the file that stores various settings that were specified during the import
         * process.
         *
         * @return File name.
         */
        static constexpr std::string_view getImportedTextureSettingsFileName() {
            return sImportedTextureSettingsFileName;
        }

        /**
         * Blocks the current thread, converts the specified texture into engine-supported formats
         * and creates new textures in the specified directory relative to `res` directory.
         *
         * @param pathToTexture               Path to the texture image to convert.
         * @param textureImportFormat         Format of the imported texture.
         * @param sPathToOutputDirRelativeRes Path to a directory relative to the `res` directory that will
         * store results, for example: `game/player/textures` (located at `res/game/player/textures`).
         * @param sOutputDirectoryName        Name of the new directory that does not exists yet but
         * will be created in the specified directory (relative to the `res`) to store the results
         * (allowed characters A-z and numbers 0-9, maximum length is 10 characters), for example: `diffuse`.
         * @param filteringPreference         Optionally you can specify a texture filter to be used
         * with this texture.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> importTexture(
            const std::filesystem::path& pathToTexture,
            TextureImportFormat textureImportFormat,
            const std::string& sPathToOutputDirRelativeRes,
            const std::string& sOutputDirectoryName,
            TextureFilteringPreference filteringPreference =
                TextureFilteringPreference::FROM_RENDER_SETTINGS);

    private:
        /** File name that we add before extension to the imported texture file. */
        static constexpr std::string_view sImportedFileName = "t";

        /** Name of the file that stores various settings that were specified during the import process. */
        static constexpr std::string_view sImportedTextureSettingsFileName = "settings";
    };
}
