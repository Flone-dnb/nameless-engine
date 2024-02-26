#include "RootSignatureGenerator.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "shader/hlsl/HlslShader.h"
#include "misc/Error.h"
#include "render/directx/DirectXRenderer.h"
#include "shader/general/DescriptorConstants.hpp"
#include "misc/Profiler.hpp"
#include "shader/general/resources/LightingShaderResourceManager.h"

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
            // Convert name to string.
            auto sName = std::string(resourceDesc.Name);

            // Find resource by name.
            const auto it = resourceNames.find(sName);
            if (it != resourceNames.end()) [[unlikely]] {
                return Error(std::format(
                    "found at least two shader resources with the same name \"{}\" - all shader "
                    "resources must have unique names",
                    resourceDesc.Name));
            }

            resourceNames.insert(std::move(sName));
        }

        // Setup variables to fill root signature info from reflection data.
        // Each root parameter can be a table, a root descriptor or a root constant.
        std::vector<RootParameter> vRootParameters;
        std::set<SamplerType> staticSamplersToBind;
        std::unordered_map<std::string, std::pair<UINT, RootParameter>> rootParameterIndices;
        std::unordered_map<std::string, size_t> rootConstantOffsets;
        bool bFoundRootConstants = false;

        // Now iterate over all shader resources and add them to root parameters.
        for (const auto& resourceDesc : vResourcesDescription) {
            if (resourceDesc.Type == D3D_SIT_CBUFFER) {
                // See if this is root constants.
                auto result = processRootConstantsIfFound(
                    pShaderReflection,
                    resourceDesc,
                    rootConstantOffsets,
                    vRootParameters,
                    rootParameterIndices);
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
                const auto bProcessedRootConstants = std::get<bool>(result);

                if (!bProcessedRootConstants) {
                    // Process as a regular cbuffer.
                    auto optionalError =
                        addCbufferRootParameter(vRootParameters, rootParameterIndices, resourceDesc);
                    if (optionalError.has_value()) [[unlikely]] {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }
                } else {
                    if (bFoundRootConstants) [[unlikely]] {
                        return Error(std::format(
                            "root constants struct was already found previously but found "
                            "another struct with root constants named \"{}\" at register {} and space {}",
                            resourceDesc.Name,
                            resourceDesc.BindPoint,
                            resourceDesc.Space));
                    }

                    bFoundRootConstants = true;
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
                    addTexture2DRootParameter(vRootParameters, rootParameterIndices, resourceDesc, false);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else if (resourceDesc.Type == D3D_SIT_STRUCTURED) {
                auto optionalError = addStructuredBufferRootParameter(
                    vRootParameters, rootParameterIndices, resourceDesc, false);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else if (resourceDesc.Type == D3D_SIT_UAV_RWSTRUCTURED) {
                auto optionalError = addStructuredBufferRootParameter(
                    vRootParameters, rootParameterIndices, resourceDesc, true);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else if (resourceDesc.Type == D3D_SIT_UAV_RWTYPED) {
                auto optionalError =
                    addTexture2DRootParameter(vRootParameters, rootParameterIndices, resourceDesc, true);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else [[unlikely]] {
                return Error(std::format(
                    "encountered unhandled shader resource type \"{}\" (not implemented)",
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
        collectedInfo.rootConstantOffsets = std::move(rootConstantOffsets);
        return collectedInfo;
    }

    std::variant<RootSignatureGenerator::Generated, Error> RootSignatureGenerator::generateGraphics(
        Renderer* pRenderer, ID3D12Device* pDevice, HlslShader* pVertexShader, HlslShader* pPixelShader) {
        PROFILE_FUNC;

        // Make sure that the vertex shader is indeed a vertex shader.
        if (pVertexShader->getShaderType() != ShaderType::VERTEX_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a vertex shader", pVertexShader->getShaderName()));
        }

        if (pPixelShader != nullptr) {
            // Make sure that the pixel shader is indeed a pixel shader.
            if (pPixelShader->getShaderType() != ShaderType::FRAGMENT_SHADER) [[unlikely]] {
                return Error(std::format(
                    "the specified shader \"{}\" is not a pixel shader", pPixelShader->getShaderName()));
            }
        }

        // Get vertex shader root signature info.
        auto pMtxVertexRootInfo = pVertexShader->getRootSignatureInfo();

        // Since pixel shader may be empty prepare a dummy mutex for scoped lock below to work.
        std::mutex mtxDummy;
        std::mutex* pPixelRootInfoMutex = &mtxDummy;

        // Get pixel shader root signature info.
        std::pair<std::mutex, std::optional<RootSignatureGenerator::CollectedInfo>>* pMtxPixelRootInfo =
            nullptr;
        if (pPixelShader != nullptr) {
            pMtxPixelRootInfo = pPixelShader->getRootSignatureInfo();
            pPixelRootInfoMutex = &pMtxPixelRootInfo->first;
        }

        // Prepare variables to create root signature.
        std::set<SamplerType> staticSamplers;
        std::unordered_map<std::string, UINT> rootParameterIndices;
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;
        std::set<std::string> addedRootParameterNames;
        std::array<UINT, static_cast<unsigned int>(SpecialRootParameterSlot::SIZE)>
            vSpecialRootParameterIndices;

        // Initialize special indices.
        for (auto& iIndex : vSpecialRootParameterIndices) {
            iIndex = std::numeric_limits<UINT>::max(); // this should cause a debug layer error if used
                                                       // uninitialized
        }

        // Lock shader root signature info.
        std::scoped_lock shaderRootSignatureInfoGuard(pMtxVertexRootInfo->first, *pPixelRootInfoMutex);

        // Make sure it's not empty.
        if (!pMtxVertexRootInfo->second.has_value()) [[unlikely]] {
            return Error(std::format(
                "unable to merge root signature of the vertex shader \"{}\" "
                "because it does have root signature info collected",
                pVertexShader->getShaderName()));
        }
        if (pMtxPixelRootInfo != nullptr) {
            if (!pMtxPixelRootInfo->second.has_value()) [[unlikely]] {
                return Error(std::format(
                    "unable to merge root signature of the pixel shader \"{}\" "
                    "because it does have root signature info collected",
                    pPixelShader->getShaderName()));
            }
        }

        // Get shaders root signature info.
        RootSignatureGenerator::CollectedInfo* pPixelRootSigInfo = nullptr;
        auto& vertexRootSigInfo = pMtxVertexRootInfo->second.value();
        if (pMtxPixelRootInfo != nullptr) {
            pPixelRootSigInfo = &(*pMtxPixelRootInfo->second);
            staticSamplers = pPixelRootSigInfo->staticSamplers;
        }

        // Do some basic checks to add parameters/samplers that don't exist in pixel shader.

        // First, add static samplers.
        for (const auto& samplerType : vertexRootSigInfo.staticSamplers) {
            // Check if we already use this sampler.
            const auto it = staticSamplers.find(samplerType);
            if (it == staticSamplers.end()) {
                // This sampler is new, add it.
                staticSamplers.insert(samplerType);
            }
        }

        // Sum vertex/pixel root parameter count.
        size_t iMaxRootParameterCount = vertexRootSigInfo.rootParameterIndices.size();
        if (pPixelRootSigInfo != nullptr) {
            iMaxRootParameterCount += pPixelRootSigInfo->rootParameterIndices.size();
        }

        // Prepare array of descriptor table ranges for root parameters to reference them since D3D
        // stores raw pointers to descriptor range objects.
        std::vector<CD3DX12_DESCRIPTOR_RANGE> vTableRanges;
        vTableRanges.reserve(iMaxRootParameterCount);
        const auto iInitialCapacity = vTableRanges.capacity();

        // Prepare to add "frame data" root parameter.
        bool bAddedFrameData = false;
        const auto addFrameDataParameter = [&](RootParameter& rootParameter) {
            // Add root parameter.
            const auto iRootParameterIndex = static_cast<unsigned int>(vRootParameters.size());
            vSpecialRootParameterIndices[static_cast<size_t>(SpecialRootParameterSlot::FRAME_DATA)] =
                iRootParameterIndex;
            vRootParameters.push_back(rootParameter.generateSingleDescriptorDescription());

            addedRootParameterNames.insert(Shader::getFrameConstantsShaderResourceName());
            rootParameterIndices[Shader::getFrameConstantsShaderResourceName()] = iRootParameterIndex;

            bAddedFrameData = true;
        };

        // Check if vertex shader uses frame data cbuffer.
        auto frameDataIndexIt =
            vertexRootSigInfo.rootParameterIndices.find(Shader::getFrameConstantsShaderResourceName());
        if (frameDataIndexIt != vertexRootSigInfo.rootParameterIndices.end()) {
            // Add root parameter.
            addFrameDataParameter(frameDataIndexIt->second.second);
        }

        if (!bAddedFrameData && pPixelRootSigInfo != nullptr) {
            // Check for pixel shader.
            frameDataIndexIt =
                pPixelRootSigInfo->rootParameterIndices.find(Shader::getFrameConstantsShaderResourceName());
            if (frameDataIndexIt != pPixelRootSigInfo->rootParameterIndices.end()) {
                // Add root parameter.
                addFrameDataParameter(frameDataIndexIt->second.second);
            }
        }

        // Prepare indices of root parameters used by shaders.
        auto shaderRootParameterIndices = vertexRootSigInfo.rootParameterIndices;
        if (pPixelRootSigInfo != nullptr) {
            // Add pixel shader parameters.
            for (const auto& [sName, pair] : pPixelRootSigInfo->rootParameterIndices) {
                // Resources with the same name store the same parameter so it's safe to overwrite
                // something here.
                shaderRootParameterIndices[sName] = pair;
            }
        }

        // Add special root parameters.
        addSpecialResourceRootParametersIfUsed(
            shaderRootParameterIndices,
            vRootParameters,
            vTableRanges,
            addedRootParameterNames,
            rootParameterIndices,
            vSpecialRootParameterIndices);

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
        if (pPixelRootSigInfo != nullptr) {
            addRootParameters(pPixelRootSigInfo->rootParameterIndices);
        }
        addRootParameters(vertexRootSigInfo.rootParameterIndices);

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
                "at least 1 shader resource (written in the shader file for shader \"{}\") is need "
                "(expected "
                "the shader to have at least `cbuffer` \"{}\")",
                pVertexShader->getShaderName(),
                Shader::getFrameConstantsShaderResourceName()));
        }

        // Merge root constants (if used).
        std::unordered_map<std::string, size_t> rootConstantOffsets = vertexRootSigInfo.rootConstantOffsets;
        if (pPixelRootSigInfo != nullptr) {
            for (const auto& [sFieldName, iOffsetInUints] : pPixelRootSigInfo->rootConstantOffsets) {
                rootConstantOffsets[sFieldName] = iOffsetInUints;
            }
        }

        // Make sure fields have unique offsets.
        std::unordered_map<size_t, std::string> fieldOffsets;
        for (const auto& [sFieldName, iOffsetInUints] : rootConstantOffsets) {
            // Make sure this offset was not used before.
            const auto it = fieldOffsets.find(iOffsetInUints);
            if (it != fieldOffsets.end()) [[unlikely]] {
                return Error(std::format(
                    "found 2 fields in root constants with different names but the same "
                    "offsets from struct start, conflicting offset {} was already used on field \"{}\" but "
                    "the field \"{}\" is also using it, this might mean that your vertex and fragment "
                    "shaders use different root constants",
                    iOffsetInUints,
                    it->second,
                    sFieldName));
            }

            // Add offset as used.
            fieldOffsets[iOffsetInUints] = sFieldName;
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
                    pMtxRenderSettings->second->getTextureFilteringQuality()));
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
        const CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
            static_cast<UINT>(vRootParameters.size()),
            vRootParameters.data(),
            static_cast<UINT>(vStaticSamplersToBind.size()),
            vStaticSamplersToBind.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // Serialize root signature in order to create it.
        ComPtr<ID3DBlob> pSerializedRootSignature = nullptr;
        ComPtr<ID3DBlob> pSerializerErrorMessage = nullptr;
        auto hResult = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            pSerializedRootSignature.GetAddressOf(),
            pSerializerErrorMessage.GetAddressOf());
        if (FAILED(hResult)) [[unlikely]] {
            if (pSerializerErrorMessage != nullptr) [[unlikely]] {
                return Error(std::string(
                    static_cast<char*>(pSerializerErrorMessage->GetBufferPointer()),
                    pSerializerErrorMessage->GetBufferSize()));
            }
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
        merged.vSpecialRootParameterIndices = vSpecialRootParameterIndices; // trivially-copyable type
        merged.rootConstantOffsets = std::move(rootConstantOffsets);
        return merged;
    }

    std::variant<RootSignatureGenerator::Generated, Error> RootSignatureGenerator::generateCompute(
        Renderer* pRenderer, ID3D12Device* pDevice, HlslShader* pComputeShader) {
        PROFILE_FUNC;

        // Make sure that the compute shader is indeed a vertex shader.
        if (pComputeShader->getShaderType() != ShaderType::COMPUTE_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a compute shader", pComputeShader->getShaderName()));
        }

        // Get used root parameters.
        auto pMtxRootInfo = pComputeShader->getRootSignatureInfo();

        // Lock info.
        std::scoped_lock shaderRootSignatureInfoGuard(pMtxRootInfo->first);

        // Make sure it's not empty.
        if (!pMtxRootInfo->second.has_value()) [[unlikely]] {
            return Error(std::format(
                "unable to generate root signature of the compute shader \"{}\" "
                "because it does have root signature info collected",
                pComputeShader->getShaderName()));
        }

        auto& shaderRootSignatureInfo = pMtxRootInfo->second.value();

        std::unordered_map<std::string, UINT> rootParameterIndices;
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters; // will be used to generate root signature
        std::set<std::string> addedRootParameterNames;

        // Prepare array of descriptor table ranges for root parameters to reference them since D3D stores
        // raw pointers to descriptor range objects.
        std::vector<CD3DX12_DESCRIPTOR_RANGE> vTableRanges;
        vTableRanges.reserve(shaderRootSignatureInfo.rootParameterIndices.size());

        // Add root parameters.
        for (const auto& [sResourceName, resourceInfo] : shaderRootSignatureInfo.rootParameterIndices) {
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

        // Create root signature description.
        // A root signature is an array of root parameters.
        const CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
            static_cast<UINT>(vRootParameters.size()),
            vRootParameters.data(),
            0,
            nullptr,
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

        Generated generated;
        generated.pRootSignature = std::move(pRootSignature);
        generated.rootParameterIndices = std::move(rootParameterIndices);
        return generated;
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

    void RootSignatureGenerator::addSpecialResourceRootParametersIfUsed(
        std::unordered_map<std::string, std::pair<UINT, RootParameter>>& shaderRootParameterIndices,
        std::vector<CD3DX12_ROOT_PARAMETER>& vRootParameters,
        std::vector<CD3DX12_DESCRIPTOR_RANGE>& vTableRanges,
        std::set<std::string>& addedRootParameterNames,
        std::unordered_map<std::string, UINT>& rootParameterIndices,
        std::array<UINT, static_cast<unsigned int>(SpecialRootParameterSlot::SIZE)>&
            vSpecialRootParameterIndices) {
        // Prepare a lambda to add some fixed root parameter indices for light resources.
        const auto addSpecialResourceRootParameter = [&](const std::string& sLightingShaderResourceName,
                                                         SpecialRootParameterSlot slot) {
            // See if this resource is used.
            const auto rootParameterIndexIt = shaderRootParameterIndices.find(sLightingShaderResourceName);
            if (rootParameterIndexIt == shaderRootParameterIndices.end()) {
                return;
            }

            // Get root parameter.
            auto& rootParameter = rootParameterIndexIt->second.second;

            // Get root parameter index.
            const auto iRootParameterIndex = static_cast<unsigned int>(vRootParameters.size());

            // Add root parameter.
            if (rootParameter.isTable()) {
                vTableRanges.push_back(rootParameter.generateTableRange());

                CD3DX12_ROOT_PARAMETER newParameter{};
                newParameter.InitAsDescriptorTable(1, &vTableRanges.back(), rootParameter.getVisibility());
                vRootParameters.push_back(newParameter);
            } else {
                vRootParameters.push_back(rootParameter.generateSingleDescriptorDescription());
            }
            addedRootParameterNames.insert(sLightingShaderResourceName);
            rootParameterIndices[sLightingShaderResourceName] = iRootParameterIndex;

            // Save special index.
            vSpecialRootParameterIndices[static_cast<size_t>(slot)] = iRootParameterIndex;
        };

        // General lighting data.
        addSpecialResourceRootParameter(
            LightingShaderResourceManager::getGeneralLightingDataShaderResourceName(),
            SpecialRootParameterSlot::GENERAL_LIGHTING);

        // Point lights array.
        addSpecialResourceRootParameter(
            LightingShaderResourceManager::getPointLightsShaderResourceName(),
            SpecialRootParameterSlot::POINT_LIGHTS);

        // Directional lights array.
        addSpecialResourceRootParameter(
            LightingShaderResourceManager::getDirectionalLightsShaderResourceName(),
            SpecialRootParameterSlot::DIRECTIONAL_LIGHTS);

        // Spotlights array.
        addSpecialResourceRootParameter(
            LightingShaderResourceManager::getSpotlightsShaderResourceName(),
            SpecialRootParameterSlot::SPOT_LIGHTS);

        // Point light index list.
        addSpecialResourceRootParameter(
            "opaquePointLightIndexList", SpecialRootParameterSlot::LIGHT_CULLING_POINT_LIGHT_INDEX_LIST);
        addSpecialResourceRootParameter(
            "transparentPointLightIndexList", SpecialRootParameterSlot::LIGHT_CULLING_POINT_LIGHT_INDEX_LIST);

        // Spotlight index list.
        addSpecialResourceRootParameter(
            "opaqueSpotLightIndexList", SpecialRootParameterSlot::LIGHT_CULLING_SPOT_LIGHT_INDEX_LIST);
        addSpecialResourceRootParameter(
            "transparentSpotLightIndexList", SpecialRootParameterSlot::LIGHT_CULLING_SPOT_LIGHT_INDEX_LIST);

        // Point light grid.
        addSpecialResourceRootParameter(
            "opaquePointLightGrid", SpecialRootParameterSlot::LIGHT_CULLING_POINT_LIGHT_GRID);
        addSpecialResourceRootParameter(
            "transparentPointLightGrid", SpecialRootParameterSlot::LIGHT_CULLING_POINT_LIGHT_GRID);

        // Spotlight grid.
        addSpecialResourceRootParameter(
            "opaqueSpotLightGrid", SpecialRootParameterSlot::LIGHT_CULLING_SPOT_LIGHT_GRID);
        addSpecialResourceRootParameter(
            "transparentSpotLightGrid", SpecialRootParameterSlot::LIGHT_CULLING_SPOT_LIGHT_GRID);

        // Add light infos for shadow pass.
        addSpecialResourceRootParameter(
            LightingShaderResourceManager::getShadowPassLightInfoArrayShaderResourceName(),
            SpecialRootParameterSlot::SHADOW_PASS_LIGHT_INFO);

        // Add root constants.
        addSpecialResourceRootParameter(sRootConstantsVariableName, SpecialRootParameterSlot::ROOT_CONSTANTS);

        // Add directional shadow maps.
        addSpecialResourceRootParameter(
            ShadowMapManager::getDirectionalShadowMapsShaderResourceName(),
            SpecialRootParameterSlot::DIRECTIONAL_SHADOW_MAPS);

        // Add spot shadow maps.
        addSpecialResourceRootParameter(
            ShadowMapManager::getSpotShadowMapsShaderResourceName(),
            SpecialRootParameterSlot::SPOT_SHADOW_MAPS);

        // Add point shadow maps.
        addSpecialResourceRootParameter(
            ShadowMapManager::getPointShadowMapsShaderResourceName(),
            SpecialRootParameterSlot::POINT_SHADOW_MAPS);
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
        const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription,
        bool bIsReadWrite) {
        // Prepare parameter description.
        auto newRootParameter = RootParameter(
            resourceDescription.BindPoint,
            resourceDescription.Space,
            bIsReadWrite ? RootParameter::Type::UAV : RootParameter::Type::SRV,
            true);

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
        const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription,
        bool bIsReadWrite) {
        auto newRootParameter = RootParameter(
            resourceDescription.BindPoint,
            resourceDescription.Space,
            bIsReadWrite ? RootParameter::Type::UAV : RootParameter::Type::SRV);

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

    std::variant<bool, Error> RootSignatureGenerator::processRootConstantsIfFound(
        const ComPtr<ID3D12ShaderReflection>& pShaderReflection,
        const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription,
        std::unordered_map<std::string, size_t>& rootConstantOffsets,
        std::vector<RootParameter>& vRootParameters,
        std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices) {
        // Make sure it's a cbuffer.
        if (resourceDescription.Type != D3D_SIT_CBUFFER) [[unlikely]] {
            return Error(std::format(
                "expected the specified resource \"{}\" to be a cbuffer", resourceDescription.Name));
        }

        // Check name.
        if (sRootConstantsVariableName != std::string(resourceDescription.Name)) {
            return false;
        }

        // Get info.
        const auto pBufferInfo = pShaderReflection->GetConstantBufferByName(resourceDescription.Name);
        if (pBufferInfo == nullptr) [[unlikely]] {
            return Error(std::format("failed to get cbuffer \"{}\" info", resourceDescription.Name));
        }

        // Get description.
        D3D12_SHADER_BUFFER_DESC bufferDesc;
        pBufferInfo->GetDesc(&bufferDesc);

        if (bufferDesc.Variables != 1) { // only struct
            return false;
        }

        // Get variable info.
        const auto pStructVariableInfo = pBufferInfo->GetVariableByIndex(0);
        if (pStructVariableInfo == nullptr) [[unlikely]] {
            return Error(std::format("failed to get cbuffer \"{}\" variable info", resourceDescription.Name));
        }

        // Get variable's type.
        const auto pStructType = pStructVariableInfo->GetType();
        if (pStructType == nullptr) [[unlikely]] {
            return Error(std::format("failed to get cbuffer \"{}\" variable type", resourceDescription.Name));
        }

        // Make sure it's a struct.
        D3D12_SHADER_TYPE_DESC structTypeDesc;
        pStructType->GetDesc(&structTypeDesc);
        if (structTypeDesc.Class != D3D_SVC_STRUCT) {
            return false;
        }

        // Check struct name.
        if (sRootConstantsTypeName != std::string(structTypeDesc.Name)) {
            return false;
        }

        // This is indeed a root constants struct.
        for (UINT i = 0; i < structTypeDesc.Members; i++) {
            // Get member variable.
            const auto pMemberType = pStructType->GetMemberTypeByIndex(i);
            if (pMemberType == nullptr) [[unlikely]] {
                return Error(std::format("failed to get member #{} of type \"{}\"", i, structTypeDesc.Name));
            }

            // Get member variable name.
            const auto pVariableName = pStructType->GetMemberTypeName(i);
            if (pVariableName == nullptr) [[unlikely]] {
                return Error(
                    std::format("failed to get name of member #{} of type \"{}\"", i, structTypeDesc.Name));
            }

            // Get member variable type.
            D3D12_SHADER_TYPE_DESC memberDesc;
            pMemberType->GetDesc(&memberDesc);

            // Make sure it's a `uint` as we expect only `uint`s.
            if (memberDesc.Type != D3D_SHADER_VARIABLE_TYPE::D3D_SVT_UINT) [[unlikely]] {
                return Error(
                    std::format("found a non uint member variable \"{}\" in root constants", pVariableName));
            }

            // Make sure member's offset is evenly divisible by sizeof(uint).
            if (memberDesc.Offset % sizeof(unsigned int) != 0) [[unlikely]] {
                return Error(std::format(
                    "expected the offset of member variable \"{}\" to be evenly divisible by sizeof(uint) in "
                    "root constants",
                    pVariableName));
            }

            // Add root constant.
            rootConstantOffsets[pVariableName] = memberDesc.Offset / sizeof(unsigned int);
        }

        // Prepare a new root parameter.
        auto newRootParameter = RootParameter(
            resourceDescription.BindPoint,
            resourceDescription.Space,
            RootParameter::Type::CONSTANTS,
            false,
            structTypeDesc.Members);

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

        return true;
    }

    RootSignatureGenerator::RootParameter::RootParameter(
        UINT iBindPoint, UINT iSpace, RootParameter::Type type, bool bIsTable, UINT iCount) {
        this->iBindPoint = iBindPoint;
        this->iSpace = iSpace;
        this->type = type;
        this->bIsTable = bIsTable;
        this->iCount = iCount;

        // Self check: make sure count is not zero.
        if (iCount == 0) [[unlikely]] {
            Error error("root parameter descriptor count cannot be zero");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
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
        case (RootParameter::Type::CONSTANTS): {
            rootParameter.InitAsConstants(iCount, iBindPoint, iSpace, visibility);
            break;
        }
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
        case (RootParameter::Type::CONSTANTS): {
            Error error("attempted to generate descriptor table range but this root parameter was "
                        "initialized as 32 bit constants");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
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

        table.Init(rangeType, iCount, iBindPoint, iSpace);

        return table;
    }

    D3D12_SHADER_VISIBILITY RootSignatureGenerator::RootParameter::getVisibility() const {
        return visibility;
    }

    bool RootSignatureGenerator::RootParameter::isTable() const { return bIsTable; }

} // namespace ne
