#include "RootSignatureGenerator.h"

// Custom.
#include "io/Logger.h"
#include "materials/hlsl/HlslShader.h"
#include "misc/Error.h"
#include "render/directx/DirectXRenderer.h"

namespace ne {
    std::variant<RootSignatureGenerator::Generated, Error> RootSignatureGenerator::generate(
        ID3D12Device* pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection) {
        // Get shader description.
        D3D12_SHADER_DESC shaderDesc;
        HRESULT hResult = pShaderReflection->GetDesc(&shaderDesc);
        if (FAILED(hResult)) {
            const Error err(hResult);
            err.showError();
        }

        // Just collect description of all resources for now.
        std::vector<D3D12_SHADER_INPUT_BIND_DESC> vResourcesDescription(shaderDesc.BoundResources);
        for (UINT iCurrentResourceIndex = 0; iCurrentResourceIndex < shaderDesc.BoundResources;
             iCurrentResourceIndex++) {
            // Get resource description.
            D3D12_SHADER_INPUT_BIND_DESC resourceDesc;
            hResult = pShaderReflection->GetResourceBindingDesc(iCurrentResourceIndex, &resourceDesc);
            if (FAILED(hResult)) [[unlikely]] {
                return Error(hResult);
            }

            // Save to array of all resources.
            vResourcesDescription[iCurrentResourceIndex] = resourceDesc;
        }

        // Make sure that names of all resources are unique.
        std::set<std::string> resourceNames;
        for (const auto& resourceDesc : vResourcesDescription) {
            auto sName = std::string(resourceDesc.Name);

            auto it = resourceNames.find(sName);
            if (it != resourceNames.end()) [[unlikely]] {
                return Error(fmt::format(
                    "found at least two shader resources with the same name \"{}\" - all shader "
                    "resources must have unique names",
                    resourceDesc.Name));
            }

            resourceNames.insert(sName);
        }

        // Root parameter can be a table, root descriptor or root constant.
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> vStaticSamplersToBind;
        std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>> rootParameterIndices;

        // Now iterate over all shader resources and add them to root parameters.
        for (const auto& resourceDesc : vResourcesDescription) {
            if (resourceDesc.Type == D3D_SIT_CBUFFER) {
                auto optionalError =
                    addCbufferRootParameter(vRootParameters, rootParameterIndices, resourceDesc);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addEntry();
                    return error;
                }
            } else if (resourceDesc.Type == D3D_SIT_SAMPLER) {
                auto result = findStaticSamplerForSamplerResource(resourceDesc);
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(result));
                    error.addEntry();
                    return error;
                }

                vStaticSamplersToBind.push_back(std::get<CD3DX12_STATIC_SAMPLER_DESC>(std::move(result)));
            } else if (resourceDesc.Type == D3D_SIT_TEXTURE) {
                // TODO
            } else [[unlikely]] {
                return Error(std::format(
                    "encountered unhandled resource type \"{}\" (not implemented)",
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

        hResult = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            pSerializedRootSignature.GetAddressOf(),
            pSerializerErrorMessage.GetAddressOf());
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        if (pSerializerErrorMessage != nullptr) [[unlikely]] {
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
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Self check: make sure root parameter indices are unique.
        std::set<UINT> indices;
        for (const auto& [sResourceName, parameterInfo] : rootParameterIndices) {
            auto it = indices.find(parameterInfo.first);
            if (it != indices.end()) [[unlikely]] {
                return Error(fmt::format(
                    "at least two resources of the generated root signature have conflicting indices"
                    "for root parameter index {} (this is a bug, please report to developers)",
                    parameterInfo.first));
            }
            indices.insert(parameterInfo.first);
        }

        // Another self check.
        if (rootParameterIndices.size() != vRootParameters.size()) [[unlikely]] {
            return Error(fmt::format(
                "sizes of generated root parameter arrays are different {} != {} (this is a bug, "
                "please report to developers)",
                rootParameterIndices.size(),
                vRootParameters.size()));
        }

        // Return results.
        Generated generated;
        generated.pRootSignature = pRootSignature;
        generated.vRootParameters = std::move(vRootParameters);
        generated.vStaticSamplers = std::move(vStaticSamplersToBind);
        generated.rootParameterIndices = std::move(rootParameterIndices);
        return generated;
    }

    std::variant<RootSignatureGenerator::Merged, Error> RootSignatureGenerator::merge(
        ID3D12Device* pDevice, HlslShader* pVertexShader, HlslShader* pPixelShader) {
        // Make sure that the vertex shader is indeed a vertex shader.
        if (pVertexShader->getShaderType() != ShaderType::VERTEX_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a vertex shader", pVertexShader->getShaderName()));
        }

        // Make sure that the pixel shader is indeed a pixel shader.
        if (pPixelShader->getShaderType() != ShaderType::PIXEL_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a pixel shader", pPixelShader->getShaderName()));
        }

        // Make sure that the shaders were compiled from the same source file.
        if (pVertexShader->getShaderSourceFileHash() != pPixelShader->getShaderSourceFileHash())
            [[unlikely]] {
            return Error(std::format(
                "the vertex shader \"{}\" and the pixel shader \"{}\" were not compiled from the same shader "
                "source file (source file hash is not equal: {} != {})",
                pVertexShader->getShaderName(),
                pPixelShader->getShaderName(),
                pVertexShader->getShaderSourceFileHash(),
                pPixelShader->getShaderSourceFileHash()));
        }

        // Get shaders' used root parameters and used static samplers.
        auto pMtxPixelRootInfo = pPixelShader->getRootSignatureInfo();
        auto pMtxVertexRootInfo = pVertexShader->getRootSignatureInfo();

        std::scoped_lock shaderRootSignatureInfoGuard(pMtxPixelRootInfo->first, pMtxVertexRootInfo->first);

        auto vStaticSamplers = pMtxPixelRootInfo->second.vStaticSamplers;
        std::unordered_map<std::string, UINT> rootParameterIndices;
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;
        std::set<std::string> addedRootParameterNames;

        // Check that vertex shader uses frame constant buffer as first root parameter.
        auto vertexFrameBufferIt =
            pMtxVertexRootInfo->second.rootParameterIndices.find(sFrameConstantBufferName);
        if (vertexFrameBufferIt == pMtxVertexRootInfo->second.rootParameterIndices.end()) [[unlikely]] {
            return Error(fmt::format(
                "expected to find `cbuffer` \"{}\" to be used in vertex shader \"{}\")",
                sFrameConstantBufferName,
                pVertexShader->getShaderName()));
        }

        // Save only first root parameter, this should be frame resources.
        static_assert(
            iFrameConstantBufferRootParameterIndex == 0,
            "frame cbuffer is expected to be the first root parameter");

        // Add first root parameter (frame constants).
        vRootParameters.push_back(vertexFrameBufferIt->second.second);
        addedRootParameterNames.insert(sFrameConstantBufferName);
        rootParameterIndices[sFrameConstantBufferName] = 0;

        // Do some basic checks to add parameters/samplers that don't exist in pixel shader.

        // First, add static samplers.
        for (const auto& sampler : pMtxVertexRootInfo->second.vStaticSamplers) {
            // Check if we already use this sampler.
            const auto it =
                std::ranges::find_if(vStaticSamplers, [&sampler](const CD3DX12_STATIC_SAMPLER_DESC& item) {
                    return item.ShaderRegister == sampler.ShaderRegister &&
                           item.RegisterSpace == sampler.RegisterSpace;
                });
            if (it == vStaticSamplers.end()) {
                // This sampler is new, add it.
                vStaticSamplers.push_back(sampler);
            }
        }

        // Then, add root parameters.
        auto addRootParameters =
            [&](const std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>>&
                    rootParametersToAdd) {
                for (const auto& [sResourceName, resourceInfo] : rootParametersToAdd) {
                    // See if we already added this resource.
                    auto it = addedRootParameterNames.find(sResourceName);
                    if (it != addedRootParameterNames.end()) {
                        continue;
                    }

                    // Add it.
                    rootParameterIndices[sResourceName] = static_cast<UINT>(vRootParameters.size());
                    vRootParameters.push_back(resourceInfo.second);
                    addedRootParameterNames.insert(sResourceName);
                }
            };
        addRootParameters(pMtxPixelRootInfo->second.rootParameterIndices);
        addRootParameters(pMtxVertexRootInfo->second.rootParameterIndices);

        // Make sure there are root parameters.
        if (vRootParameters.empty()) [[unlikely]] {
            return Error(fmt::format(
                "at least 1 shader resource (written in the shader file for shader \"{}\") is need (expected "
                "the shader to have at least `cbuffer` \"{}\")",
                pVertexShader->getShaderName(),
                sFrameConstantBufferName));
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

        if (pSerializerErrorMessage != nullptr) {
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

        Merged merged;
        merged.pRootSignature = std::move(pRootSignature);
        merged.rootParameterIndices = std::move(rootParameterIndices);
        return merged;
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

    std::optional<Error> RootSignatureGenerator::addUniquePairResourceNameRootParameterIndex(
        std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>>& mapToAddTo,
        const std::string& sResourceName,
        UINT iRootParameterIndex,
        const CD3DX12_ROOT_PARAMETER& parameter) {
        // See if resource with this name already exists.
        auto it = mapToAddTo.find(sResourceName);
        if (it != mapToAddTo.end()) [[unlikely]] {
            return Error(fmt::format(
                "found two shader resources with equal names - \"{}\" (see shader file), all shader "
                "resources must have unique names",
                sResourceName));
        }

        // Add to map.
        mapToAddTo[sResourceName] = std::make_pair(iRootParameterIndex, parameter);

        return {};
    }

    std::optional<Error> RootSignatureGenerator::addCbufferRootParameter(
        std::vector<CD3DX12_ROOT_PARAMETER>& vRootParameters,
        std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>>& rootParameterIndices,
        const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription) {
        // Prepare root parameter description.
        auto newRootParameter = CD3DX12_ROOT_PARAMETER{};
        newRootParameter.InitAsConstantBufferView(resourceDescription.BindPoint, resourceDescription.Space);

        // Make sure this resource name is unique, save its root index.
        auto optionalError = addUniquePairResourceNameRootParameterIndex(
            rootParameterIndices,
            resourceDescription.Name,
            static_cast<UINT>(vRootParameters.size()),
            newRootParameter);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        // Add to root parameters.
        vRootParameters.push_back(newRootParameter);

        return {};
    }
} // namespace ne
