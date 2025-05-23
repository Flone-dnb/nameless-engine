﻿#include "DirectXPso.h"

// Custom.
#include "shader/hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#include "shader/hlsl/RootSignatureGenerator.h"
#include "io/Logger.h"
#include "render/RenderSettings.h"
#include "shader/hlsl/format/HlslVertexFormatDescription.h"
#include "render/general/resource/shadow/ShadowMapManager.h"
#include "render/directx/descriptors/DirectXDescriptorHeap.h"

namespace ne {
    DirectXPso::DirectXPso(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        std::unique_ptr<PipelineConfiguration> pPipelineConfiguration)
        : Pipeline(pRenderer, pPipelineManager, std::move(pPipelineConfiguration)) {}

    DirectXPso::~DirectXPso() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Destroy pipeline objects if they are valid.
        if (!mtxInternalResources.second.bIsReadyForUsage) {
            return;
        }

        // Make sure the renderer is no longer using this PSO or its resources.
        Logger::get().info(std::format(
            "waiting for the GPU to finish work up to this point before destroying a PSO with id \"{}\"",
            getPipelineIdentifier()));
        getRenderer()->waitForGpuToFinishWorkUpToThisPoint();
    }

    std::variant<std::shared_ptr<DirectXPso>, Error> DirectXPso::createGraphicsPso(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        std::unique_ptr<PipelineConfiguration> pPipelineConfiguration) {
        // Create PSO.
        auto pPso = std::shared_ptr<DirectXPso>(
            new DirectXPso(pRenderer, pPipelineManager, std::move(pPipelineConfiguration)));

        // Generate graphics PSO.
        auto optionalError = pPso->generateGraphicsPso();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pPso;
    }

    std::variant<std::shared_ptr<DirectXPso>, Error> DirectXPso::createComputePso(
        Renderer* pRenderer, PipelineManager* pPipelineManager, const std::string& sComputeShaderName) {
        // Create PSO.
        auto pPso = std::shared_ptr<DirectXPso>(new DirectXPso(
            pRenderer, pPipelineManager, std::make_unique<ComputePipelineConfiguration>(sComputeShaderName)));

        // Generate compute PSO.
        auto optionalError = pPso->generateComputePso();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pPso;
    }

    std::variant<unsigned int, Error>
    DirectXPso::getRootParameterIndex(const std::string& sShaderResourceName) {
        std::scoped_lock psoGuard(mtxInternalResources.first);

        auto it = mtxInternalResources.second.rootParameterIndices.find(sShaderResourceName);
        if (it == mtxInternalResources.second.rootParameterIndices.end()) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader resource by the specified name \"{}\", make sure the resource name "
                "is correct and that this resource is actually being used inside of your shader (otherwise "
                "the shader resource might be optimized out and the engine will not be able to see it)",
                sShaderResourceName));
        }

        return it->second;
    }

    std::optional<Error> DirectXPso::recreateInternalResources() {
        std::scoped_lock guard(mtxInternalResources.first);

        if (!mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "PSO was requested to release internal PSO resources but internal resources are already "
                "released, ignoring this request");
            return {};
        }

        // Release graphics PSO.
        auto iNewRefCount = mtxInternalResources.second.pPso.Reset();
        if (iNewRefCount != 0) {
            return Error(std::format(
                "internal graphics PSO was requested to be released from the "
                "memory but it's still being referenced (new ref count: {}) (PSO ID: {})",
                iNewRefCount,
                getPipelineIdentifier()));
        }

        // Release root signature.
        mtxInternalResources.second.pRootSignature.Reset();
        // `CreateRootSignature` can return a pointer to the existing root signature (for example
        // a pointer to the root signature of some shader) if arguments
        // for creation were the same as in the previous call.
        // Because of this we don't check returned ref count for zero since we don't know
        // whether it's safe to compare it to zero or not.
        //        iNewRefCount = mtxInternalResources.second.pRootSignature.Reset();
        //        if (iNewRefCount != 0) {
        //            return Error(std::format(
        //                "internal root signature was requested to be released from the "
        //                "memory but it's still being referenced (new ref count: {}) (PSO ID: {})",
        //                iNewRefCount,
        //                getUniquePsoIdentifier()));
        //        }

        mtxInternalResources.second.globalShaderResourceCbvs.clear();
        mtxInternalResources.second.globalShaderResourceSrvs.clear();
        mtxInternalResources.second.descriptorRangesToBind.clear();
        mtxInternalResources.second.rootParameterIndices.clear();

#if defined(DEBUG)
        static_assert(
            sizeof(InternalResources) == 408, // NOLINT: current struct size
            "release new resources here");
#endif

        // Done releasing resources.
        mtxInternalResources.second.bIsReadyForUsage = false;

        // Recreate internal PSO and root signature.
        auto optionalError = generateGraphicsPso();
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return {};
    }

    std::optional<Error> DirectXPso::generateGraphicsPso() {
        // Prepare pipeline type.
        const auto bDepthOnlyPipeline = getConfiguration()->getFragmentShaderName().empty();

        // Make sure pixel shader is specified when blending is enabled.
        if (getConfiguration()->isPixelBlendingEnabled() && bDepthOnlyPipeline) [[unlikely]] {
            return Error(std::format(
                "unable to create a pipeline with pixel blending because pixel shader is not specified "
                "(vertex shader \"{}\")",
                getConfiguration()->getVertexShaderName()));
        }

        // Get settings.
        const auto mtxRenderSettings = getRenderer()->getRenderSettings();
        std::scoped_lock resourcesGuard(mtxInternalResources.first, *mtxRenderSettings.first);

        // Get AA setting.
        const auto antialiasingQuality = mtxRenderSettings.second->getAntialiasingQuality();

        // Make sure the pipeline is not initialized yet.
        if (mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "PSO was requested to generate internal PSO resources but internal resources are already "
                "created, ignoring this request");
            return {};
        }

        // Assign vertex shader.
        if (addShader(std::string(getConfiguration()->getVertexShaderName()))) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader named \"{}\"", getConfiguration()->getVertexShaderName()));
        }

        // Assign pixel shader.
        if (!bDepthOnlyPipeline) {
            if (addShader(std::string(getConfiguration()->getFragmentShaderName()))) [[unlikely]] {
                return Error(std::format(
                    "unable to find a shader named \"{}\"", getConfiguration()->getFragmentShaderName()));
            }
        }

        // Get assigned vertex shader.
        const auto pVertexShaderPack = findShader(ShaderType::VERTEX_SHADER);
        std::set<ShaderMacro> fullVertexShaderConfiguration;
        auto pVertexShader = std::dynamic_pointer_cast<HlslShader>(
            pVertexShaderPack->getShader(getConfiguration()->getRequiredVertexShaderMacros()));

        // Get vertex shader bytecode.
        auto shaderBytecode = pVertexShader->getCompiledBlob();
        if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        const ComPtr<IDxcBlob> pVertexShaderBytecode = std::get<ComPtr<IDxcBlob>>(std::move(shaderBytecode));

        // Prepare pixel shader.
        std::shared_ptr<HlslShader> pPixelShader;
        ComPtr<IDxcBlob> pPixelShaderBytecode;
        std::set<ShaderMacro> fullPixelShaderConfiguration;
        if (!bDepthOnlyPipeline) {
            // Get assigned pixel shader.
            const auto pPixelShaderPack = findShader(ShaderType::FRAGMENT_SHADER);
            pPixelShader = std::dynamic_pointer_cast<HlslShader>(
                pPixelShaderPack->getShader(getConfiguration()->getRequiredFragmentShaderMacros()));

            // Get pixel shader bytecode and generate its root signature.
            shaderBytecode = pPixelShader->getCompiledBlob();
            if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
                auto err = std::get<Error>(std::move(shaderBytecode));
                err.addCurrentLocationToErrorStack();
                return err;
            }
            pPixelShaderBytecode = std::get<ComPtr<IDxcBlob>>(std::move(shaderBytecode));
        }

        // Get DirectX renderer.
        auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(getRenderer());
        if (pDirectXRenderer == nullptr) [[unlikely]] {
            return Error("expected a DirectX renderer");
        }

        // Generate root signature from shaders.
        auto result = RootSignatureGenerator::generateGraphics(
            pDirectXRenderer, pVertexShader.get(), pPixelShader.get());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto generatedRootSignature = std::get<RootSignatureGenerator::Generated>(std::move(result));
        mtxInternalResources.second.pRootSignature = std::move(generatedRootSignature.pRootSignature);
        mtxInternalResources.second.rootParameterIndices =
            std::move(generatedRootSignature.rootParameterIndices);
        mtxInternalResources.second.vSpecialRootParameterIndices =
            generatedRootSignature.vSpecialRootParameterIndices; // trivially-copyable type

        // Prepare to create a PSO using these shaders.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // Get vertex format.
        const auto optionalVertexFormat = pVertexShader->getVertexFormat();
        if (!optionalVertexFormat.has_value()) [[unlikely]] {
            return Error(std::format(
                "expected vertex format to be set for vertex shader \"{}\"", pVertexShader->getShaderName()));
        }
        const auto pVertexFormat =
            HlslVertexFormatDescription::createDescription(optionalVertexFormat.value());

        // Setup input layout and shaders.
        const auto vInputLayout = pVertexFormat->getShaderInputElementDescription();
        psoDesc.InputLayout = {vInputLayout.data(), static_cast<UINT>(vInputLayout.size())};
        psoDesc.pRootSignature = mtxInternalResources.second.pRootSignature.Get();
        psoDesc.VS = {pVertexShaderBytecode->GetBufferPointer(), pVertexShaderBytecode->GetBufferSize()};
        if (!bDepthOnlyPipeline) {
            psoDesc.PS = {pPixelShaderBytecode->GetBufferPointer(), pPixelShaderBytecode->GetBufferSize()};
        }

        // Setup rasterizer description.
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode =
            getConfiguration()->isPixelBlendingEnabled() ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

        // Specify depth bias settings.
        if (getConfiguration()->isDepthBiasEnabled()) {
            psoDesc.RasterizerState.DepthBias = ShadowMapManager::getShadowPassDepthBias();
            psoDesc.RasterizerState.DepthBiasClamp = 0.0F;
            psoDesc.RasterizerState.SlopeScaledDepthBias = ShadowMapManager::getShadowPassDepthSlopeFactor();
        }

        // Setup pixel blend description (if needed).
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        if (getConfiguration()->isPixelBlendingEnabled()) {
            D3D12_RENDER_TARGET_BLEND_DESC blendDesc;
            blendDesc.BlendEnable = 1;
            blendDesc.LogicOpEnable = 0;
            blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
            blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
            blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            psoDesc.BlendState.RenderTarget[0] = blendDesc;
            psoDesc.BlendState.AlphaToCoverageEnable = 0;
        }

        // Describe depth stencil state.
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        if (!getConfiguration()->isDepthBiasEnabled() && !bDepthOnlyPipeline) {
            // This is main pass pipeline.

            // Disable depth writes because depth buffer will be filled during depth prepass
            // and depth buffer will be in read-only state during the main pass.
            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

            // Keep depth-testing enabled but add `equal` to depth comparison because some depths will be
            // equal now since we render the same thing again.
            psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        }

        // Finalize PSO description.
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // MSAA.
        if (getConfiguration()->isDepthBiasEnabled()) {
            // No multisampling when drawing to shadow maps.
            psoDesc.RasterizerState.MultisampleEnable = 0;
            psoDesc.SampleDesc.Count = 1;
            psoDesc.SampleDesc.Quality = 0;
        } else {
            psoDesc.RasterizerState.MultisampleEnable =
                static_cast<int>(antialiasingQuality != AntialiasingQuality::DISABLED);

            psoDesc.SampleDesc.Count =
                antialiasingQuality.has_value() ? static_cast<unsigned int>(*antialiasingQuality) : 1;

            if (!antialiasingQuality.has_value() || *antialiasingQuality == AntialiasingQuality::DISABLED) {
                psoDesc.SampleDesc.Quality = 0;
            } else {
                psoDesc.SampleDesc.Quality = pDirectXRenderer->getMsaaQualityLevel() - 1;
            }
        }

        // DSV format.
        if (getConfiguration()->isDepthBiasEnabled()) {
            psoDesc.DSVFormat = DirectXRenderer::getShadowMapFormat();
        } else {
            psoDesc.DSVFormat = DirectXRenderer::getDepthStencilBufferFormat();
        }

        // Specify render target.
        if (!bDepthOnlyPipeline) {
            psoDesc.NumRenderTargets = 1;
            auto shadowUsage = getConfiguration()->getShadowMappingUsage();
            if (shadowUsage.has_value() && *shadowUsage == PipelineShadowMappingUsage::POINT_LIGHTS) {
                psoDesc.RTVFormats[0] = DirectXRenderer::getShadowMappingPointLightColorTargetFormat();
            } else {
                psoDesc.RTVFormats[0] = DirectXRenderer::getBackBufferFormat();
            }
        } else {
            psoDesc.NumRenderTargets = 0;
        }

        // Create PSO.
        HRESULT hResult = pDirectXRenderer->getD3dDevice()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(mtxInternalResources.second.pPso.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Set new root constants.
        setShaderConstants(generatedRootSignature.rootConstantOffsets);

        // Done.
        mtxInternalResources.second.bIsReadyForUsage = true;

        return {};
    }

    std::optional<Error> DirectXPso::generateComputePso() {
        // Make sure the pipeline is not initialized yet.
        if (mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "PSO was requested to generate internal PSO resources but internal resources are already "
                "created, ignoring this request");
            return {};
        }

        // Assign new shaders.
        const bool bComputeShaderNotFound =
            addShader(std::string(getConfiguration()->getComputeShaderName()));

        // Make sure that shader was found.
        if (bComputeShaderNotFound) [[unlikely]] {
            return Error(std::format(
                "shader \"{}\" was not found in Shader Manager", getConfiguration()->getComputeShaderName()));
        }

        // Get assigned shader pack.
        const auto pComputeShaderPack = findShader(ShaderType::COMPUTE_SHADER);

        // Get shader.
        std::set<ShaderMacro> fullComputeShaderConfiguration;
        auto pComputeShader = std::dynamic_pointer_cast<HlslShader>(pComputeShaderPack->getShader({}));

        // Get DirectX renderer.
        auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(getRenderer());
        if (pDirectXRenderer == nullptr) [[unlikely]] {
            return Error("expected a DirectX renderer");
        }

        // Get vertex shader bytecode and generate its root signature.
        auto shaderBytecode = pComputeShader->getCompiledBlob();
        if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        const ComPtr<IDxcBlob> pComputeShaderBytecode = std::get<ComPtr<IDxcBlob>>(std::move(shaderBytecode));

        // Generate one root signature from both shaders.
        auto result = RootSignatureGenerator::generateCompute(pDirectXRenderer, pComputeShader.get());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto generatedRootSignature = std::get<RootSignatureGenerator::Generated>(std::move(result));
        mtxInternalResources.second.pRootSignature = std::move(generatedRootSignature.pRootSignature);
        mtxInternalResources.second.rootParameterIndices =
            std::move(generatedRootSignature.rootParameterIndices);
        mtxInternalResources.second.vSpecialRootParameterIndices =
            generatedRootSignature.vSpecialRootParameterIndices; // trivially-copyable type

        // Prepare to create a PSO using these shaders.
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = mtxInternalResources.second.pRootSignature.Get();
        psoDesc.CS = {pComputeShaderBytecode->GetBufferPointer(), pComputeShaderBytecode->GetBufferSize()};

        // Create PSO.
        HRESULT hResult = pDirectXRenderer->getD3dDevice()->CreateComputePipelineState(
            &psoDesc, IID_PPV_ARGS(mtxInternalResources.second.pPso.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Done.
        mtxInternalResources.second.bIsReadyForUsage = true;

        return {};
    }
}
