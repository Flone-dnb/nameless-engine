#pragma once

// STL.
#include <filesystem>
#include <memory>

// Custom.
#include <variant>

#include "misc/Error.h"
#include "shaders/ShaderDescription.hpp"

namespace ne {
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
        IShader(const IShader &) = delete;
        IShader &operator=(const IShader &) = delete;

        /**
         * Compiles a shader.
         *
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        template <typename ShaderFormat>
        requires std::derived_from<ShaderFormat, IShader>
        static std::variant<
            std::unique_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(ShaderDescription shaderDescription) {
            return ShaderFormat::compileShader(std::move(shaderDescription));
        }

        virtual ~IShader() = default;

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
    };
} // namespace ne
