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
         * Generates Root Signature based on HLSL code.
         *
         * @param pDevice           DirectX device.
         * @param pShaderReflection Reflection from compiled HLSL shader.
         *
         * @return Error if something was wrong, otherwise generated root signature.
         */
        static std::variant<ComPtr<ID3D12RootSignature>, Error> generateRootSignature(
            ComPtr<ID3D12Device>& pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection);

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
