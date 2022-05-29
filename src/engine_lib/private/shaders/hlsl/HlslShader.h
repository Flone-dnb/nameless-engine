#pragma once

// Custom.
#include "shaders/IShader.h"

// DXC.
#include "dxc/dxcapi.h"

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
            std::shared_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(const ShaderDescription& shaderDescription);

        /**
         * Loads compiled bytecode from disk and stores it in memory.
         * Subsequent calls to this function will just copy pointer
         * to bytecode from memory (no disk loading will happen).
         *
         * @return Compiled shader blob.
         */
        std::variant<ComPtr<IDxcBlob>, Error> getCompiledBlob();

    private:
        ComPtr<IDxcBlob> pCompiledBlob;

        // Clear shader cache if changing shader model.
        static constexpr std::wstring_view sVertexShaderModel = L"vs_6_0";
        static constexpr std::wstring_view sPixelShaderModel = L"ps_6_0";
        static constexpr std::wstring_view sComputeShaderModel = L"cs_6_0";
    };
} // namespace ne
