#include "RootSignatureGenerator.h"

// Custom.
#include "misc/Error.h"
#include "render/directx/DirectXRenderer.h"

namespace ne {
    std::variant<ComPtr<ID3D12RootSignature>, Error> RootSignatureGenerator::generateRootSignature(
        ComPtr<ID3D12Device>& pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection) {
        // Root parameter can be a table, root descriptor or root constant.
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> vStaticSamplersToBind;

        // Iterate over all shader resources.
        for (UINT iCurrentResourceIndex = 0;; iCurrentResourceIndex++) {
            D3D12_SHADER_INPUT_BIND_DESC resourceDesc;
            const HRESULT hResult =
                pShaderReflection->GetResourceBindingDesc(iCurrentResourceIndex, &resourceDesc);
            if (hResult == E_INVALIDARG) {
                break; // no more resources
            }
            if (FAILED(hResult)) {
                const Error err(hResult);
                err.showError();
            }

            // TODO: see if finds resources from included HLSL files

            if (resourceDesc.Type == D3D_SIT_CBUFFER) {
                auto newRootParameter = CD3DX12_ROOT_PARAMETER{};
                newRootParameter.InitAsConstantBufferView(resourceDesc.BindPoint, resourceDesc.Space);
                vRootParameters.push_back(newRootParameter);
            } else if (resourceDesc.Type == D3D_SIT_SAMPLER) {
                auto result = findStaticSamplerForSamplerResource(resourceDesc);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addEntry();
                    return error;
                }

                vStaticSamplersToBind.push_back(std::get<CD3DX12_STATIC_SAMPLER_DESC>(std::move(result)));
            } else {
                return Error(std::format(
                    "encountered unhandled resource type {} (not implemented)",
                    static_cast<int>(resourceDesc.Type)));
            }
        }

        // Create root signature description.
        // A root signature is an array of root parameters.
        const CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
            static_cast<UINT>(vRootParameters.size()),
            vRootParameters.data(),
            static_cast<UINT>(vStaticSamplersToBind.size()),
            vStaticSamplersToBind.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // Serialize root signature in order to create it.
        ComPtr<ID3DBlob> pSerializedRootSignature = nullptr;
        ComPtr<ID3DBlob> pSerializerErrorMessage = nullptr;

        HRESULT hResult = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            pSerializedRootSignature.GetAddressOf(),
            pSerializerErrorMessage.GetAddressOf());
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        if (pSerializerErrorMessage) {
            return Error(std::string(
                static_cast<char*>(pSerializerErrorMessage->GetBufferPointer()),
                pSerializerErrorMessage->GetBufferSize()));
        }

        // Create root signature.
        ComPtr<ID3D12RootSignature> pRootSignature;
        hResult = pDevice->CreateRootSignature(
            0,
            pSerializedRootSignature->GetBufferPointer(),
            pSerializedRootSignature->GetBufferSize(),
            IID_PPV_ARGS(&pRootSignature));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return pRootSignature;
    }

    std::variant<CD3DX12_STATIC_SAMPLER_DESC, Error>
    RootSignatureGenerator::findStaticSamplerForSamplerResource(
        const D3D12_SHADER_INPUT_BIND_DESC& samplerResourceDescription) {
        const auto vStaticSamplers = DirectXRenderer::getStaticTextureSamplers();
        const auto sResourceName = std::string(samplerResourceDescription.Name);

        D3D12_FILTER currentSamplerFilter;
        if (sResourceName.contains("Point") || sResourceName.contains("point") ||
            sResourceName.contains("POINT")) {
            currentSamplerFilter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        } else if (
            sResourceName.contains("Linear") || sResourceName.contains("linear") ||
            sResourceName.contains("LINEAR")) {
            currentSamplerFilter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        } else if (
            sResourceName.contains("Anisotropic") || sResourceName.contains("anisotropic") ||
            sResourceName.contains("ANISOTROPIC")) {
            currentSamplerFilter = D3D12_FILTER_ANISOTROPIC;
        } else if (
            sResourceName.contains("Shadow") || sResourceName.contains("shadow") ||
            sResourceName.contains("SHADOW")) {
            currentSamplerFilter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        } else {
            return Error(std::format(
                "static sampler for the specified \"{}\" sampler resource is not found, "
                "please add some keywords to the resource name like \"point\", \"anisotropic\", \"linear\" "
                "or \"shadow\", for example: \"samplerAnisotropicWrap\"",
                sResourceName));
        }

        // Find static sampler for this sampler resource.
        for (const auto& sampler : vStaticSamplers) {
            if (sampler.Filter == currentSamplerFilter) {
                if (samplerResourceDescription.BindPoint != sampler.ShaderRegister) {
                    return Error(std::format(
                        "\"{}\" sampler register should be {} instead of {}",
                        sResourceName,
                        sampler.ShaderRegister,
                        samplerResourceDescription.BindPoint));
                }
                if (samplerResourceDescription.Space != sampler.RegisterSpace) {
                    return Error(std::format(
                        "\"{}\" sampler register space should be {} instead of {}",
                        sResourceName,
                        sampler.RegisterSpace,
                        samplerResourceDescription.Space));
                }
                return sampler;
            }
        }

        return Error(std::format(
            "static sampler with filter {} is not found", static_cast<int>(currentSamplerFilter)));
    }
} // namespace ne
