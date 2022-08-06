#include "RootSignatureGenerator.h"

// Custom.
#include "io/Logger.h"
#include "shaders/hlsl/HlslShader.h"
#include "misc/Error.h"
#include "render/directx/DirectXRenderer.h"

namespace ne {
    std::variant<
        std::tuple<
            ComPtr<ID3D12RootSignature>,
            std::vector<CD3DX12_ROOT_PARAMETER>,
            std::vector<CD3DX12_STATIC_SAMPLER_DESC>>,
        Error>
    RootSignatureGenerator::generate(
        ID3D12Device* pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection) {
        // Root parameter can be a table, root descriptor or root constant.
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> vStaticSamplersToBind;

        // Texture resources.
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

        return std::make_tuple(pRootSignature, vRootParameters, vStaticSamplersToBind);
    }

    std::variant<ComPtr<ID3D12RootSignature>, Error> RootSignatureGenerator::merge(
        ID3D12Device* pDevice, const HlslShader* pVertexShader, const HlslShader* pPixelShader) {
        // Check that vertex shader is indeed a vertex shader.
        if (pVertexShader->getShaderType() != ShaderType::VERTEX_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a vertex shader", pVertexShader->getShaderName()));
        }

        // Check that pixel shader is indeed a pixel shader.
        if (pPixelShader->getShaderType() != ShaderType::PIXEL_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a pixel shader", pPixelShader->getShaderName()));
        }

        // Check that shaders were compiled from the same source file.
        if (pVertexShader->getShaderSourceFileHash() != pPixelShader->getShaderSourceFileHash())
            [[unlikely]] {
            return Error(std::format(
                "the vertex shader \"{}\" and the pixel shader \"{}\" were not compiled from one shader "
                "source file (source file hash is not equal: {} != {})",
                pVertexShader->getShaderName(),
                pPixelShader->getShaderName(),
                pVertexShader->getShaderSourceFileHash(),
                pPixelShader->getShaderSourceFileHash()));
        }

        // Get shaders' root parameters and used static samplers.
        auto vRootParameters = pPixelShader->getShaderRootParameters();
        auto vStaticSamplers = pPixelShader->getShaderStaticSamplers();
        const auto vRootParametersToAdd = pVertexShader->getShaderRootParameters();
        const auto vStaticSamplersToAdd = pVertexShader->getShaderStaticSamplers();

        // Do some basic checks to add parameters/samplers that don't exist in vertex shader.

        // First, add static samplers.
        for (const auto& sampler : vStaticSamplersToAdd) {
            // Check if we already use this sampler.
            const auto it =
                std::ranges::find_if(vStaticSamplers, [&sampler](const CD3DX12_STATIC_SAMPLER_DESC& item) {
                    return item.ShaderRegister == sampler.ShaderRegister &&
                           item.RegisterSpace == sampler.RegisterSpace;
                });
            if (it == vStaticSamplers.end()) {
                // This sampler is new, add it.
                vStaticSamplers.push_back(sampler);
            } // else: we already use this static sampler
        }

        // Then, add root parameters.
        for (const auto& parameter : vRootParametersToAdd) {
            // Check if we already use this root parameter.
            const auto it =
                std::ranges::find_if(vRootParameters, [&parameter](const CD3DX12_ROOT_PARAMETER& item) {
                    if (parameter.ShaderVisibility != item.ShaderVisibility ||
                        parameter.ParameterType != item.ParameterType) {
                        return false;
                    }

                    if (parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
                        if (parameter.Constants.RegisterSpace == item.Constants.RegisterSpace &&
                            parameter.Constants.ShaderRegister == item.Constants.ShaderRegister) {
                            return true;
                        }
                    } else if (
                        parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ||
                        parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
                        parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV) {
                        if (parameter.Descriptor.RegisterSpace == item.Descriptor.RegisterSpace &&
                            parameter.Descriptor.ShaderRegister == item.Descriptor.ShaderRegister) {
                            return true;
                        }
                    } else if (parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
                        if (parameter.DescriptorTable.NumDescriptorRanges !=
                            item.DescriptorTable.NumDescriptorRanges) {
                            return false;
                        }

                        for (UINT i = 0; i < parameter.DescriptorTable.NumDescriptorRanges; i++) {
                            bool bFound = false;
                            for (UINT j = 0; j < item.DescriptorTable.NumDescriptorRanges; j++) {
                                if (parameter.DescriptorTable.pDescriptorRanges[i].BaseShaderRegister ==
                                        item.DescriptorTable.pDescriptorRanges[j].BaseShaderRegister &&
                                    parameter.DescriptorTable.pDescriptorRanges[i].RegisterSpace ==
                                        item.DescriptorTable.pDescriptorRanges[j].RegisterSpace) {
                                    bFound = true;
                                    break;
                                }
                            }

                            if (!bFound)
                                return false;
                        }

                        return true;
                    } else {
                        Logger::get().error(
                            std::format(
                                "unhandled root signature parameter type {}",
                                static_cast<int>(parameter.ParameterType)),
                            "");
                        return false;
                    }

                    return false;
                });
            if (it == vRootParameters.end()) {
                // This parameter is new, add it.
                vRootParameters.push_back(parameter);
            } // else: we already use this parameter
        }

        // Create root signature description.
        // A root signature is an array of root parameters.
        const CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
            static_cast<UINT>(vRootParameters.size()),
            vRootParameters.data(),
            static_cast<UINT>(vStaticSamplers.size()),
            vStaticSamplers.data(),
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
