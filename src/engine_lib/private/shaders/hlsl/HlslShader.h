#pragma once

// Custom.
#include "shaders/IShader.h"

// DXC
#include "dxc/d3d12shader.h"
#include "dxc/dxcapi.h"
#pragma comment(lib, "dxcompiler.lib")

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    /**
     * Represents compiled HLSL shader.
     */
    class HlslShader : public IShader {
    public:
        /**
         * Constructor.
         *
         * @param pathToCompiledShader Path to compiled shader blob on disk.
         */
        HlslShader(std::filesystem::path pathToCompiledShader);

        HlslShader() = delete;
        HlslShader(const HlslShader&) = delete;
        HlslShader& operator=(const HlslShader&) = delete;

        virtual ~HlslShader() override {}

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
        static std::variant<
            std::unique_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(const ShaderDescription& shaderDescription);

        /**
         * Returns compiled shader.
         *
         * @return Compiled shader blob.
         */
        ComPtr<IDxcBlob> getCompiledBlob();

    private:
        ComPtr<IDxcBlob> pCompiledBlob;
    };
} // namespace ne
