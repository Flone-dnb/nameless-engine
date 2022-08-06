#pragma once

// STL.
#include <variant>

// Custom.
#include "misc/Error.h"

// OS.
#include <wrl/client.h>

// DXC.
#include "directx/d3dx12.h"
#include <DirectXShaderCompiler/inc/d3d12shader.h>

namespace ne {
    class HlslShader;

    using namespace Microsoft::WRL;

    /**
     * Generates Root Signature based on HLSL code.
     */
    class RootSignatureGenerator {
    public:
        RootSignatureGenerator() = delete;
        RootSignatureGenerator(const RootSignatureGenerator&) = delete;
        RootSignatureGenerator& operator=(const RootSignatureGenerator&) = delete;

        /**
         * Generates root signature based on HLSL code reflection.
         *
         * @param pDevice           DirectX device.
         * @param pShaderReflection Reflection from compiled HLSL shader.
         *
         * @return Error if something was wrong, otherwise generated root signature with used parameters.
         */
        static std::variant<
            std::tuple<
                ComPtr<ID3D12RootSignature>,
                std::vector<CD3DX12_ROOT_PARAMETER>,
                std::vector<CD3DX12_STATIC_SAMPLER_DESC>>,
            Error>
        generate(ID3D12Device* pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection);

        /**
         * Merges vertex and pixel shader root signatures into one new root signature
         * that can be used in pipeline state object.
         *
         * @param pDevice       DirectX device.
         * @param pVertexShader Vertex shader.
         * @param pPixelShader  Pixel shader.
         *
         * @remark Shaders must be compiled from one shader source file.
         *
         * @return Error if something went wrong, otherwise generated root signature.
         */
        static std::variant<ComPtr<ID3D12RootSignature>, Error>
        merge(ID3D12Device* pDevice, const HlslShader* pVertexShader, const HlslShader* pPixelShader);

    private:
        /**
         * Finds static sampler for the specified sampler resource.
         *
         * @param samplerResourceDescription HLSL resource description.
         *
         * @return Error if static sampler is not found, otherwise found static sampler.
         */
        static std::variant<CD3DX12_STATIC_SAMPLER_DESC, Error>
        findStaticSamplerForSamplerResource(const D3D12_SHADER_INPUT_BIND_DESC& samplerResourceDescription);
    };
} // namespace ne
