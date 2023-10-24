﻿#include "RootSignatureGenerator.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "materials/hlsl/HlslShader.h"
#include "misc/Error.h"
#include "render/directx/DirectXRenderer.h"
#include "materials/DescriptorConstants.hpp"
#include "misc/Profiler.hpp"
#include "materials/resources/LightingShaderResourceManager.h"

namespace ne {
    std::variant<RootSignatureGenerator::CollectedInfo, Error>
    RootSignatureGenerator::collectInfoFromReflection(
        ID3D12Device* pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection) {
        PROFILE_FUNC;

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
                return Error(std::format(
                    "found at least two shader resources with the same name \"{}\" - all shader "
                    "resources must have unique names",
                    resourceDesc.Name));
            }

            resourceNames.insert(sName);
        }

        // Setup variables to fill root signature info from reflection data.
        // Each root parameter can be a table, a root descriptor or a root constant.
        std::vector<RootParameter> vRootParameters;
        std::set<SamplerType> staticSamplersToBind;
        std::unordered_map<std::string, std::pair<UINT, RootParameter>> rootParameterIndices;

        // Now iterate over all shader resources and add them to root parameters.
        for (const auto& resourceDesc : vResourcesDescription) {
            if (resourceDesc.Type == D3D_SIT_CBUFFER) {
                auto optionalError =
                    addCbufferRootParameter(vRootParameters, rootParameterIndices, resourceDesc);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else if (resourceDesc.Type == D3D_SIT_SAMPLER) {
                auto result = findStaticSamplerForSamplerResource(resourceDesc);
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
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
                auto optionalError =
                    addTexture2DRootParameter(vRootParameters, rootParameterIndices, resourceDesc);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else if (resourceDesc.Type == D3D_SIT_STRUCTURED) {
                auto optionalError =
                    addStructuredBufferRootParameter(vRootParameters, rootParameterIndices, resourceDesc);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else [[unlikely]] {
                return Error(std::format(
                    "encountered unhandled resource type \"{}\" (not implemented)",
                    static_cast<int>(resourceDesc.Type)));
            }
        }

        // Self check: make sure root parameter indices are unique.
        std::set<UINT> indices;
        for (const auto& [sResourceName, parameterInfo] : rootParameterIndices) {
            auto it = indices.find(parameterInfo.first);
            if (it != indices.end()) [[unlikely]] {
                return Error(std::format(
                    "at least two resources of the generated root signature have conflicting indices"
                    "for root parameter index {} (this is a bug, please report to developers)",
                    parameterInfo.first));
            }
            indices.insert(parameterInfo.first);
        }

        // Another self check.
        if (rootParameterIndices.size() != vRootParameters.size()) [[unlikely]] {
            return Error(std::format(
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
        PROFILE_FUNC;

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

        // Get shaders used root parameters and used static samplers.
        auto pMtxPixelRootInfo = pPixelShader->getRootSignatureInfo();
        auto pMtxVertexRootInfo = pVertexShader->getRootSignatureInfo();

        // Lock info.
        std::scoped_lock shaderRootSignatureInfoGuard(pMtxPixelRootInfo->first, pMtxVertexRootInfo->first);

        // Make sure it's not empty.
        if (!pMtxPixelRootInfo->second.has_value()) [[unlikely]] {
            return Error(std::format(
                "unable to merge root signature of the pixel shader \"{}\" "
                "because it does have root signature info collected",
                pPixelShader->getShaderName()));
        }
        if (!pMtxVertexRootInfo->second.has_value()) [[unlikely]] {
            return Error(std::format(
                "unable to merge root signature of the vertex shader \"{}\" "
                "because it does have root signature info collected",
                pVertexShader->getShaderName()));
        }

        auto& pixelShaderRootSignatureInfo = pMtxPixelRootInfo->second.value();
        auto& vertexShaderRootSignatureInfo = pMtxVertexRootInfo->second.value();
        auto staticSamplers = pixelShaderRootSignatureInfo.staticSamplers;

        std::unordered_map<std::string, UINT> rootParameterIndices;
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters; // will be used to generate root signature
        std::set<std::string> addedRootParameterNames;

        // Check that vertex shader uses frame constant buffer as first root parameter.
        const auto vertexFrameBufferIt =
            vertexShaderRootSignatureInfo.rootParameterIndices.find(sFrameConstantBufferName);
        if (vertexFrameBufferIt == vertexShaderRootSignatureInfo.rootParameterIndices.end()) [[unlikely]] {
            return Error(std::format(
                "expected to find `cbuffer` \"{}\" to be used in vertex shader \"{}\"",
                sFrameConstantBufferName,
                pVertexShader->getShaderName()));
        }

        // Add first root parameter (frame constants).
        static_assert(
            iFrameConstantBufferRootParameterIndex == 0, "change order in which we add to `vRootParameters`");
        vRootParameters.push_back(vertexFrameBufferIt->second.second.generateSingleDescriptorDescription());
        addedRootParameterNames.insert(sFrameConstantBufferName);
        rootParameterIndices[sFrameConstantBufferName] = iFrameConstantBufferRootParameterIndex;

        // Prepare a lambda to add some fixed root parameter indices.
        auto addLightingResourceRootParameter = [&](const std::string& sLightingShaderResourceName,
                                                    UINT iLightingResourceRootParameterIndex) {
            // See if this resource is used.
            const auto rootParameterIndexIt =
                pixelShaderRootSignatureInfo.rootParameterIndices.find(sLightingShaderResourceName);

            if (rootParameterIndexIt != pixelShaderRootSignatureInfo.rootParameterIndices.end()) {
                // Add root parameter.
                vRootParameters.push_back(
                    rootParameterIndexIt->second.second.generateSingleDescriptorDescription());
                addedRootParameterNames.insert(sLightingShaderResourceName);
                rootParameterIndices[sLightingShaderResourceName] = iLightingResourceRootParameterIndex;
            }
        };

        // Check if general lighting data is used and then assign it a fixed root parameter index.
        static_assert(
            iGeneralLightingConstantBufferRootParameterIndex == 1,
            "change order in which we add to `vRootParameters`");
        addLightingResourceRootParameter(
            LightingShaderResourceManager::getGeneralLightingDataShaderResourceName(),
            iGeneralLightingConstantBufferRootParameterIndex);

        // Check if point lights are used and then assign it a fixed root parameter index.
        static_assert(
            iPointLightsBufferRootParameterIndex == 2, "change order in which we add to `vRootParameters`");
        addLightingResourceRootParameter(
            LightingShaderResourceManager::getPointLightsShaderResourceName(),
            iPointLightsBufferRootParameterIndex);

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

        // Prepare array of descriptor table ranges for root parameters to reference them since D3D stores
        // raw pointers to descriptor range objects.
        std::vector<CD3DX12_DESCRIPTOR_RANGE> vTableRanges;
        vTableRanges.reserve(
            pixelShaderRootSignatureInfo.rootParameterIndices.size() +
            vertexShaderRootSignatureInfo.rootParameterIndices.size());
        const auto iInitialCapacity = vTableRanges.capacity();

        // Then, add other root parameters.
        auto addRootParameters =
            [&](const std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParametersToAdd) {
                for (const auto& [sResourceName, resourceInfo] : rootParametersToAdd) {
                    // See if we already added this resource.
                    auto it = addedRootParameterNames.find(sResourceName);
                    if (it != addedRootParameterNames.end()) {
                        continue;
                    }

                    // Add this resource.
                    rootParameterIndices[sResourceName] = static_cast<UINT>(vRootParameters.size());
                    addedRootParameterNames.insert(sResourceName);

                    if (resourceInfo.second.isTable()) {
                        // Add descriptor table.
                        vTableRanges.push_back(resourceInfo.second.generateTableRange());
                        CD3DX12_ROOT_PARAMETER newParameter{};
                        newParameter.InitAsDescriptorTable(
                            1, &vTableRanges.back(), resourceInfo.second.getVisibility());
                        vRootParameters.push_back(newParameter);
                    } else {
                        // Add single descriptor.
                        vRootParameters.push_back(resourceInfo.second.generateSingleDescriptorDescription());
                    }
                }
            };
        addRootParameters(pixelShaderRootSignatureInfo.rootParameterIndices);
        addRootParameters(vertexShaderRootSignatureInfo.rootParameterIndices);

        // Self check: make sure ranges were not moved to other place in memory.
        if (vTableRanges.capacity() != iInitialCapacity) [[unlikely]] {
            Error error(std::format(
                "table range array capacity changed from {} to {}",
                iInitialCapacity,
                vTableRanges.capacity()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure there are root parameters.
        if (vRootParameters.empty()) [[unlikely]] {
            return Error(std::format(
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
                return Error(std::format(
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
                return Error(std::format(
                    "expected the sampler \"{}\" to use shader register {} instead of {}",
                    sResourceName,
                    static_cast<UINT>(StaticSamplerShaderRegister::COMPARISON),
                    samplerResourceDescription.BindPoint));
            }

            typeToReturn = SamplerType::COMPARISON;
        } else [[unlikely]] {
            return Error(std::format(
                "expected sampler \"{}\" to be named either as \"{}\" (for `SamplerState` type) or as "
                "\"{}\" (for `SamplerComparisonState` type)",
                sResourceName,
                sBasicSamplerName,
                sComparisonSamplerName));
        }

        // Make sure shader register space is correct.
        if (samplerResourceDescription.Space != HlslShader::getStaticSamplerShaderRegisterSpace())
            [[unlikely]] {
            return Error(std::format(
                "expected the sampler \"{}\" to use shader register space {} instead of {}",
                sResourceName,
                HlslShader::getStaticSamplerShaderRegisterSpace(),
                samplerResourceDescription.Space));
        }

        return typeToReturn;
    }

    std::optional<Error> RootSignatureGenerator::addUniquePairResourceNameRootParameterIndex(
        std::unordered_map<std::string, std::pair<UINT, RootParameter>>& mapToAddTo,
        const std::string& sResourceName,
        UINT iRootParameterIndex,
        const RootParameter& parameter) {
        // See if resource with this name already exists.
        auto it = mapToAddTo.find(sResourceName);
        if (it != mapToAddTo.end()) [[unlikely]] {
            return Error(std::format(
                "found two shader resources with equal names - \"{}\" (see shader file), all shader "
                "resources must have unique names",
                sResourceName));
        }

        // Add to map.
        mapToAddTo[sResourceName] =
            std::pair<UINT, RootSignatureGenerator::RootParameter>{iRootParameterIndex, parameter};

        return {};
    }

    std::optional<Error> RootSignatureGenerator::addCbufferRootParameter(
        std::vector<RootParameter>& vRootParameters,
        std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices,
        const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription) {
        // Prepare root parameter description.
        auto newRootParameter =
            RootParameter(resourceDescription.BindPoint, resourceDescription.Space, RootParameter::Type::CBV);

        // Make sure this resource name is unique, save its root index.
        auto optionalError = addUniquePairResourceNameRootParameterIndex(
            rootParameterIndices,
            resourceDescription.Name,
            static_cast<UINT>(vRootParameters.size()),
            newRootParameter);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Add to root parameters.
        vRootParameters.push_back(newRootParameter);

        return {};
    }

    std::optional<Error> RootSignatureGenerator::addTexture2DRootParameter(
        std::vector<RootParameter>& vRootParameters,
        std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices,
        const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription) {
        auto newRootParameter = RootParameter(
            resourceDescription.BindPoint, resourceDescription.Space, RootParameter::Type::SRV, true);

        // Make sure this resource name is unique, save its root index.
        auto optionalError = addUniquePairResourceNameRootParameterIndex(
            rootParameterIndices,
            resourceDescription.Name,
            static_cast<UINT>(vRootParameters.size()),
            newRootParameter);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Add to root parameters.
        vRootParameters.push_back(newRootParameter);

        return {};
    }

    std::optional<Error> RootSignatureGenerator::addStructuredBufferRootParameter(
        std::vector<RootParameter>& vRootParameters,
        std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices,
        const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription) {
        auto newRootParameter =
            RootParameter(resourceDescription.BindPoint, resourceDescription.Space, RootParameter::Type::SRV);

        // Make sure this resource name is unique, save its root index.
        auto optionalError = addUniquePairResourceNameRootParameterIndex(
            rootParameterIndices,
            resourceDescription.Name,
            static_cast<UINT>(vRootParameters.size()),
            newRootParameter);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Add to root parameters.
        vRootParameters.push_back(newRootParameter);

        return {};
    }

    RootSignatureGenerator::RootParameter::RootParameter(
        UINT iBindPoint, UINT iSpace, RootParameter::Type type, bool bIsTable, UINT iDescriptorCount) {
        this->iBindPoint = iBindPoint;
        this->iSpace = iSpace;
        this->type = type;
        this->bIsTable = bIsTable;
        this->iDescriptorCount = iDescriptorCount;

        // Self check: make sure descriptor count is not zero.
        if (iDescriptorCount == 0) [[unlikely]] {
            Error error("root parameter descriptor count cannot be zero");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Set visibility.
        if (type == RootParameter::Type::SRV) {
            visibility = D3D12_SHADER_VISIBILITY_PIXEL;
        }
    }

    CD3DX12_ROOT_PARAMETER
    RootSignatureGenerator::RootParameter::generateSingleDescriptorDescription() const {
        // Self check: make sure it's not a descriptor table.
        if (bIsTable) [[unlikely]] {
            Error error(
                "attempted to generate descriptor description but this root parameter was initialized "
                "as descriptor table");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        auto rootParameter = CD3DX12_ROOT_PARAMETER{};

        switch (type) {
        case (RootParameter::Type::CBV): {
            rootParameter.InitAsConstantBufferView(iBindPoint, iSpace, visibility);
            break;
        }
        case (RootParameter::Type::SRV): {
            rootParameter.InitAsShaderResourceView(iBindPoint, iSpace, visibility);
            break;
        }
        case (RootParameter::Type::UAV): {
            rootParameter.InitAsUnorderedAccessView(iBindPoint, iSpace, visibility);
            break;
        }
        default: {
            Error error("unhandled case");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
        }

        return rootParameter;
    }

    CD3DX12_DESCRIPTOR_RANGE RootSignatureGenerator::RootParameter::generateTableRange() const {
        // Self check: make sure it's a descriptor table.
        if (!bIsTable) [[unlikely]] {
            Error error(
                "attempted to generate descriptor table range but this root parameter was initialized "
                "as a single descriptor (not a table)");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        auto table = CD3DX12_DESCRIPTOR_RANGE{};

        D3D12_DESCRIPTOR_RANGE_TYPE rangeType;
        switch (type) {
        case (RootParameter::Type::CBV): {
            rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            break;
        }
        case (RootParameter::Type::SRV): {
            rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            break;
        }
        case (RootParameter::Type::UAV): {
            rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            break;
        }
        default: {
            Error error("unhandled case");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
        }

        table.Init(rangeType, iDescriptorCount, iBindPoint, iSpace);

        return table;
    }

    D3D12_SHADER_VISIBILITY RootSignatureGenerator::RootParameter::getVisibility() const {
        return visibility;
    }

    bool RootSignatureGenerator::RootParameter::isTable() const { return bIsTable; }

} // namespace ne
