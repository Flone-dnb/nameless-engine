#pragma once

// STL.
#include <filesystem>
#include <memory>

// Custom.
#include <variant>

#include "misc/Error.h"
#include "shaders/ShaderDescription.h"

namespace ne {
    class IRenderer;
    /**
     * Interface class for different types/formats of shaders.
     */
    class IShader {
    public:
        /**
         * Constructor.
         *
         * @param pathToCompiledShader Path to compiled shader blob on disk.
         */
        IShader(std::filesystem::path pathToCompiledShader);

        IShader() = delete;
        IShader(const IShader&) = delete;
        IShader& operator=(const IShader&) = delete;

        /**
         * Compiles a shader.
         *
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         * @param pRenderer Used renderer.
         *
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::unique_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(ShaderDescription& shaderDescription, IRenderer* pRenderer);

        virtual ~IShader() = default;

        /**
         * Returns path to the directory used to store shader cache.
         *
         * @return Path to shader cache directory.
         */
        static std::filesystem::path getPathToShaderCacheDirectory();

    protected:
        /**
         * Returns path to compiled shader blob on disk.
         *
         * @return Path to compiled shader.
         */
        std::filesystem::path getPathToCompiledShader();

    private:
        /** Path to compiled shader. */
        std::filesystem::path pathToCompiledShader;

        /** Directory name to store compiled shaders. */
        static constexpr std::string_view sShaderCacheDirectoryName = "shader_cache";
    };
} // namespace ne
