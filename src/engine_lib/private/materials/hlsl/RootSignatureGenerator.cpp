#include "RootSignatureGenerator.h"

// Custom.
#include "io/Logger.h"
#include "materials/hlsl/HlslShader.h"
#include "misc/Error.h"
#include "render/directx/DirectXRenderer.h"

// External.
#include "fmt/core.h"

namespace ne {
    std::variant<RootSignatureGenerator::CollectedInfo, Error>
    RootSignatureGenerator::collectInfoFromReflection(
        ID3D12Device* pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection) {
        // Get shader description from reflection.
        D3D12_SHADER_DESC shaderDesc;
        HRESULT hResult = pShaderReflection->GetDesc(&shaderDesc);
        if (FAILED(hResult)) [[unlikely]] {
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

        // Setup variables to fill root signature info from reflection data.
        // Each root parameter can be a table, a root descriptor or a root constant.
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;
        std::set<SamplerType> staticSamplersToBind;
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

                // Make sure there is no sampler of this type.
                const auto newSamplerType = std::get<SamplerType>(result);
                auto it = staticSamplersToBind.find(newSamplerType);
                if (it != staticSamplersToBind.end()) [[unlikely]] {
                    return Error("unexpected to find 2 samplers of the same type");
                }

                staticSamplersToBind.insert(std::get<SamplerType>(result));
            } else if (resourceDesc.Type == D3D_SIT_TEXTURE) {
                // TODO
            } else [[unlikely]] {
                return Error(fmt::format(
                    "encountered unhandled resource type \"{}\" (not implemented)",
                    static_cast<int>(resourceDesc.Type)));
            }
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
        CollectedInfo collectedInfo;
        collectedInfo.vRootParameters = std::move(vRootParameters);
        collectedInfo.staticSamplers = std::move(staticSamplersToBind);
        collectedInfo.rootParameterIndices = std::move(rootParameterIndices);
        return collectedInfo;
    }

    std::variant<RootSignatureGenerator::Generated, Error> RootSignatureGenerator::generate(
        Renderer* pRenderer, ID3D12Device* pDevice, HlslShader* pVertexShader, HlslShader* pPixelShader) {
        // Make sure that the vertex shader is indeed a vertex shader.
        if (pVertexShader->getShaderType() != ShaderType::VERTEX_SHADER) [[unlikely]] {
            return Error(fmt::format(
                "the specified shader \"{}\" is not a vertex shader", pVertexShader->getShaderName()));
        }

        // Make sure that the pixel shader is indeed a pixel shader.
        if (pPixelShader->getShaderType() != ShaderType::PIXEL_SHADER) [[unlikely]] {
            return Error(fmt::format(
                "the specified shader \"{}\" is not a pixel shader", pPixelShader->getShaderName()));
        }

        // Make sure that the shaders were compiled from the same source file.
        if (pVertexShader->getShaderSourceFileHash() != pPixelShader->getShaderSourceFileHash())
            [[unlikely]] {
            return Error(fmt::format(
                "the vertex shader \"{}\" and the pixel shader \"{}\" were not compiled from the same shader "
                "source file (source file hash is not equal: {} != {})",
                pVertexShader->getShaderName(),
                pPixelShader->getShaderName(),
                pVertexShader->getShaderSourceFileHash(),
                pPixelShader->getShaderSourceFileHash()));
        }

        // Get pixel shader used root parameters and used static samplers.
        auto pMtxPixelRootInfo = pPixelShader->getRootSignatureInfo();
        if (!pMtxPixelRootInfo->second.has_value()) [[unlikely]] {
            return Error(fmt::format(
                "unable to merge root signature of the pixel shader \"{}\" "
                "because it does have root signature generated",
                pPixelShader->getShaderName()));
        }

        // Get vertex shader used root parameters and used static samplers.
        auto pMtxVertexRootInfo = pVertexShader->getRootSignatureInfo();
        if (!pMtxVertexRootInfo->second.has_value()) [[unlikely]] {
            return Error(fmt::format(
                "unable to merge root signature of the vertex shader \"{}\" "
                "because it does have root signature generated",
                pVertexShader->getShaderName()));
        }

        std::scoped_lock shaderRootSignatureInfoGuard(pMtxPixelRootInfo->first, pMtxVertexRootInfo->first);

        auto& pixelShaderRootSignatureInfo = pMtxPixelRootInfo->second.value();
        auto& vertexShaderRootSignatureInfo = pMtxVertexRootInfo->second.value();
        auto staticSamplers = pixelShaderRootSignatureInfo.staticSamplers;

        std::unordered_map<std::string, UINT> rootParameterIndices;
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;
        std::set<std::string> addedRootParameterNames;

        // Check that vertex shader uses frame constant buffer as first root parameter.
        auto vertexFrameBufferIt =
            vertexShaderRootSignatureInfo.rootParameterIndices.find(sFrameConstantBufferName);
        if (vertexFrameBufferIt == vertexShaderRootSignatureInfo.rootParameterIndices.end()) [[unlikely]] {
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
        for (const auto& samplerType : vertexShaderRootSignatureInfo.staticSamplers) {
            // Check if we already use this sampler.
            const auto it = staticSamplers.find(samplerType);
            if (it == staticSamplers.end()) {
                // This sampler is new, add it.
                staticSamplers.insert(samplerType);
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
        addRootParameters(pixelShaderRootSignatureInfo.rootParameterIndices);
        addRootParameters(vertexShaderRootSignatureInfo.rootParameterIndices);

        // Make sure there are root parameters.
        if (vRootParameters.empty()) [[unlikely]] {
            return Error(fmt::format(
                "at least 1 shader resource (written in the shader file for shader \"{}\") is need (expected "
                "the shader to have at least `cbuffer` \"{}\")",
                pVertexShader->getShaderName(),
                sFrameConstantBufferName));
        }

        // Get current render settings to query texture filtering for static sampler.
        const auto pMtxRenderSettings = pRenderer->getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first); // keep locked until finished

        // Collect static sampler description.
        std::vector<D3D12_STATIC_SAMPLER_DESC> vStaticSamplersToBind;
        for (const auto& samplerType : staticSamplers) {
            switch (samplerType) {
            case (SamplerType::BASIC): {
                vStaticSamplersToBind.push_back(HlslShader::getStaticSamplerDescription(
                    pMtxRenderSettings->second->getTextureFilteringMode()));
                break;
            }
            case (SamplerType::COMPARISON): {
                vStaticSamplersToBind.push_back(HlslShader::getStaticComparisonSamplerDescription());
                break;
            }
            default: {
                Error error("unhandled case");
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
                break;
            }
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

        Generated merged;
        merged.pRootSignature = std::move(pRootSignature);
        merged.rootParameterIndices = std::move(rootParameterIndices);
        return merged;
    }

    std::variant<SamplerType, Error> RootSignatureGenerator::findStaticSamplerForSamplerResource(
        const D3D12_SHADER_INPUT_BIND_DESC& samplerResourceDescription) {
        const auto sResourceName = std::string(samplerResourceDescription.Name);

        constexpr std::string_view sBasicSamplerName = "textureSampler";
        constexpr std::string_view sComparisonSamplerName = "shadowSampler";

        SamplerType typeToReturn = SamplerType::BASIC;
        if (sResourceName == sBasicSamplerName) {
            // Make sure shader register is correct.
            if (samplerResourceDescription.BindPoint != static_cast<UINT>(StaticSamplerShaderRegister::BASIC))
                [[unlikely]] {
                return Error(fmt::format(
                    "expected the sampler \"{}\" to use shader register {} instead of {}",
                    sResourceName,
                    static_cast<UINT>(StaticSamplerShaderRegister::BASIC),
                    samplerResourceDescription.BindPoint));
            }

            typeToReturn = SamplerType::BASIC;
        } else if (sResourceName == sComparisonSamplerName) {
            // Make sure shader register is correct.
            if (samplerResourceDescription.BindPoint !=
                static_cast<UINT>(StaticSamplerShaderRegister::COMPARISON)) [[unlikely]] {
                return Error(fmt::format(
                    "expected the sampler \"{}\" to use shader register {} instead of {}",
                    sResourceName,
                    static_cast<UINT>(StaticSamplerShaderRegister::COMPARISON),
                    samplerResourceDescription.BindPoint));
            }

            typeToReturn = SamplerType::COMPARISON;
        } else [[unlikely]] {
            return Error(fmt::format(
                "expected sampler \"{}\" to be named either as \"{}\" (for `SamplerState` type) or as "
                "\"{}\" (for `SamplerComparisonState` type)",
                sResourceName,
                sBasicSamplerName,
                sComparisonSamplerName));
        }

        // Make sure shader register space is correct.
        if (samplerResourceDescription.Space != HlslShader::getStaticSamplerShaderRegisterSpace())
            [[unlikely]] {
            return Error(fmt::format(
                "expected the sampler \"{}\" to use shader register space {} instead of {}",
                sResourceName,
                HlslShader::getStaticSamplerShaderRegisterSpace(),
                samplerResourceDescription.Space));
        }

        return typeToReturn;
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
