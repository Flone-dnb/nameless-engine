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

        struct TextureResourceTable {
            UINT iRegisterSpace = 0;
            UINT iTextureResourceCount = 0;
            UINT iTexturesBaseShaderRegister = UINT_MAX;
        };
        std::vector<TextureResourceTable> vTextureResources;

        // Get shader description.
        D3D12_SHADER_DESC shaderDesc;
        HRESULT hResult = pShaderReflection->GetDesc(&shaderDesc);
        if (FAILED(hResult)) {
            const Error err(hResult);
            err.showError();
        }

        // Iterate over all shader resources.
        for (UINT iCurrentResourceIndex = 0; iCurrentResourceIndex < shaderDesc.BoundResources;
             iCurrentResourceIndex++) {
            D3D12_SHADER_INPUT_BIND_DESC resourceDesc;
            hResult = pShaderReflection->GetResourceBindingDesc(iCurrentResourceIndex, &resourceDesc);
            if (FAILED(hResult)) {
                const Error err(hResult);
                err.showError();
            }

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
            } else if (resourceDesc.Type == D3D_SIT_TEXTURE) {
                auto texIt = std::ranges::find_if(vTextureResources, [&](const TextureResourceTable& table) {
                    return table.iRegisterSpace == resourceDesc.Space;
                });
                if (texIt == vTextureResources.end()) {
                    TextureResourceTable newTextureTable;
                    newTextureTable.iRegisterSpace = resourceDesc.Space;
                    newTextureTable.iTextureResourceCount = 1;
                    newTextureTable.iTexturesBaseShaderRegister = resourceDesc.BindPoint;
                    vTextureResources.push_back(newTextureTable);
                } else {
                    texIt->iTextureResourceCount += 1;
                    if (resourceDesc.BindPoint < texIt->iTexturesBaseShaderRegister) {
                        texIt->iTexturesBaseShaderRegister = resourceDesc.BindPoint;
                    } else if (
                        resourceDesc.BindPoint >=
                        texIt->iTexturesBaseShaderRegister + texIt->iTextureResourceCount) {
                        return Error(std::format(
                            "invalid texture register {} on texture resource \"{}\", "
                            "expected to find a contiguous range of texture registers (1, 2, 3..., "
                            "not 1, 2, 4...) in the same register space",
                            resourceDesc.BindPoint,
                            resourceDesc.Name));
                    }
                }
            } else {
                return Error(std::format(
                    "encountered unhandled resource type {} (not implemented)",
                    static_cast<int>(resourceDesc.Type)));
            }
        }

        // Create descriptor table for texture resources.
        std::vector<CD3DX12_DESCRIPTOR_RANGE> vTextureDescriptorRanges;
        for (const auto& textureTable : vTextureResources) {
            CD3DX12_DESCRIPTOR_RANGE textureResourceDescriptors;
            textureResourceDescriptors.Init(
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                textureTable.iTextureResourceCount,
                textureTable.iTexturesBaseShaderRegister,
                textureTable.iRegisterSpace,
                D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // auto increment total count
            vTextureDescriptorRanges.push_back(textureResourceDescriptors);
        }
        if (!vTextureDescriptorRanges.empty()) {
            auto newRootParameter = CD3DX12_ROOT_PARAMETER{};
            newRootParameter.InitAsDescriptorTable(
                static_cast<UINT>(vTextureDescriptorRanges.size()), vTextureDescriptorRanges.data());
            vRootParameters.push_back(newRootParameter);
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

        hResult = D3D12SerializeRootSignature(
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
