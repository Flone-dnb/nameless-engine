#pragma once

// Standard.
#include <filesystem>
#include <optional>
#include <functional>

// Custom.
#include <misc/Error.h>

namespace ne {

    typedef bool (*ImportTextureCallback)(
        float progress, unsigned long long iNotUsed1, unsigned long long iNotUsed2);

    /** Controls texture loading. */
    class TextureManager {
    public:
        /**
         * Blocks the current thread, converts the specified texture into engine-supported formats
         * and creates new textures in the specified directory relative to `res` directory.
         *
         * @param pathToTexture               Path to the texture image to convert.
         * @param sPathToOutputDirRelativeRes Path to a directory relative to the `res` directory that will
         * store results, for example: `game/player/textures` (located at `res/game/player/textures`).
         * @param sOutputDirectoryName        Name of the new directory that does not exists yet but
         * will be created in the specified directory (relative to the `res`) to store the results
         * (allowed characters A-z and numbers 0-9, maximum length is 10 characters), for example: `diffuse`.
         * @param pCompressionStateCallback   Callback that will be regularly called to tell the progress
         * of the compression operation (tells progress percent as `float` parameter). Returned value is
         * used to cancel the operation, return `false` to continue and `true` to abort, note that
         * when you abort the compression operation this function will return `Error`.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> importTexture(
            const std::filesystem::path& pathToTexture,
            const std::string& sPathToOutputDirRelativeRes,
            const std::string& sOutputDirectoryName,
            ImportTextureCallback pCompressionStateCallback);
    };
}
