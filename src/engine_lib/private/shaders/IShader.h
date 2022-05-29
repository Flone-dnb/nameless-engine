#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>

// Custom.
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
         * @param sShaderName          Unique name of this shader.
         * @param shaderType           Type of this shader.
         */
        IShader(
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType);

        IShader() = delete;
        IShader(const IShader&) = delete;
        IShader& operator=(const IShader&) = delete;

        virtual ~IShader() = default;

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
            std::shared_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(ShaderDescription& shaderDescription, IRenderer* pRenderer);

        /**
         * Returns unique name of this shader.
         *
         * @return Unique name of this shader.
         */
        std::string getShaderName() const;

        /**
         * Returns path to the directory used to store shader cache.
         *
         * @return Path to shader cache directory.
         */
        static std::filesystem::path getPathToShaderCacheDirectory();

        /**
         * Returns type of this shader.
         *
         * @return Shader type.
         */
        ShaderType getShaderType() const;

        /**
         * Releases underlying shader bytecode from memory (this object will not be deleted).
         * Next time this shader will be needed it will be loaded from disk.
         */
        virtual void releaseBytecodeFromMemory() = 0;

    protected:
        /**
         * Returns path to compiled shader blob on disk.
         *
         * @return Path to compiled shader.
         */
        std::filesystem::path getPathToCompiledShader();

    private:
        /** Unique shader name received from ShaderManager. */
        std::string sShaderName;

        /** Type of this shader. */
        ShaderType shaderType;

        /** Path to compiled shader. */
        std::filesystem::path pathToCompiledShader;

        /** Directory name to store compiled shaders. */
        static constexpr std::string_view sShaderCacheDirectoryName = "shader_cache";
    };
} // namespace ne
