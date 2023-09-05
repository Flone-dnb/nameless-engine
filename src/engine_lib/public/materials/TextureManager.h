#pragma once

// Standard.
#include <filesystem>
#include <optional>
#include <functional>

// Custom.
#include <misc/Error.h>

namespace ne {

#if defined(WIN32)
    typedef bool (*ImportTextureCallback)(
        float progress, unsigned long long iNotUsed1, unsigned long long iNotUsed2);
#else
    typedef bool (*ImportTextureCallback)(float progress, int* pNotUsed1, int* pNotUsed2);
#endif

    /** Describes texture format. */
    enum class TextureType {
        DIFFUSE_TEXTURE,
        NORMAL_TEXTURE,
        HDR_TEXTURE,
    };

    /** Controls texture loading. */
    class TextureManager {
    public:
        /**
         * Blocks the current thread, converts the specified texture into engine-supported formats
         * and creates new textures in the specified directory relative to `res` directory.
         *
         * @param pathToTexture               Path to the texture image to convert.
         * @param textureType                 Type of the texture image to convert.
         * @param sPathToOutputDirRelativeRes Path to a directory relative to the `res` directory that will
         * store results, for example: `game/player/textures` (located at `res/game/player/textures`).
         * @param sOutputDirectoryName        Name of the new directory that does not exists yet but
         * will be created in the specified directory (relative to the `res`) to store the results
         * (allowed characters A-z and numbers 0-9, maximum length is 10 characters), for example: `diffuse`.
         * @param pCompressionStateCallback   Callback that will be regularly called to tell the progress
         * of the compression operation (tells progress percent as `float` parameter). Returned value is
         * used to cancel the operation, return `false` to continue and `true` to abort, note that
         * when you abort the compression operation this function will return `Error`. Note that this
         * callback will be called multiple times after reaching 100% `float` parameter.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> importTexture(
            const std::filesystem::path& pathToTexture,
            TextureType textureType,
            const std::string& sPathToOutputDirRelativeRes,
            const std::string& sOutputDirectoryName,
            ImportTextureCallback pCompressionStateCallback);
    };
}
