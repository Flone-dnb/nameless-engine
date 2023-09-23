#pragma once

// Standard.
#include <filesystem>
#include <optional>
#include <functional>

// Custom.
#include "misc/Error.h"

namespace ne {
    /**
     * Provides static functions for importing files in special formats (such as GLTF/GLB) as meshes,
     * textures, etc. into engine formats (such as nodes).
     */
    class MeshImporter {
    public:
        MeshImporter() = delete;

        /**
         * Imports a file in a special format (such as GTLF/GLB) and converts information
         * from the file to a new node tree with materials and textures (if the specified file defines them).
         *
         * @param pathToFile                  Path to the file to import.
         * @param sPathToOutputDirRelativeRes Path to a directory relative to the `res` directory that will
         * store results, for example: `game/player` (located at `res/game/player`).
         * @param sOutputFileName             Name of the new file (without extension) that does not exists
         * yet but will be created in the specified directory (allowed characters A-z and numbers 0-9, maximum
         * length is 10 characters), for example: `mesh`.
         * @param onProgress                  Callback that will be called to report progress (percent) and
         * some text description of the current import stage.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> importMesh(
            const std::filesystem::path& pathToFile,
            const std::string& sPathToOutputDirRelativeRes,
            const std::string& sOutputFileName,
            const std::function<void(float, const std::string&)>& onProgress);
    };
}
