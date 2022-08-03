#pragma once

// STL.
#include <filesystem>

// Custom.
#include "misc/ProjectPaths.h"

namespace ne {
    /** Provides a set of static function related to filesystem paths to store shaders. */
    class ShaderFilesystemPaths {
    public:
        ShaderFilesystemPaths() = delete;
        ShaderFilesystemPaths(const ShaderFilesystemPaths&) = delete;
        ShaderFilesystemPaths& operator=(const ShaderFilesystemPaths&) = delete;

        /**
         * Returns path to the directory used to store shader cache, for example:
         * ".../nameless-engine/<project_name>/shader_cache/".
         *
         * @return Path to shader cache directory (created if not existed before).
         */
        static std::filesystem::path getPathToShaderCacheDirectory() {
            std::filesystem::path basePath = ProjectPaths::getDirectoryForCompiledShaders();

            if (!std::filesystem::exists(basePath)) {
                std::filesystem::create_directories(basePath);
            }
            return basePath;
        }

        /**
         * Returns base file name used in shader cache. This name
         * is used within a shader specific directory (like ".../shader_cache/engine.default"),
         * to name different files using different extensions (like "shader", "shader.pdb",
         * "shader.reflection" and etc.).
         *
         * @return Base file name.
         */
        static std::string getShaderCacheBaseFileName() { return sShaderCacheBaseFileName; }

    private:
        /** Base name of the file used to store shader cache. */
        static constexpr auto sShaderCacheBaseFileName = "shader";
    };
} // namespace ne
