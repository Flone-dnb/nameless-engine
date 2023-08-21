﻿#include "DirectXPso.h"

// Custom.
#include "materials/hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#include "materials/hlsl/RootSignatureGenerator.h"
#include "io/Logger.h"
#include "render/RenderSettings.h"

namespace ne {
    DirectXPso::DirectXPso(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending)
        : Pipeline(pRenderer, pPipelineManager, sVertexShaderName, sPixelShaderName, bUsePixelBlending) {}

    DirectXPso::~DirectXPso() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Destroy pipeline objects if they are valid.
        if (!mtxInternalResources.second.bIsReadyForUsage) {
            return;
        }

        // Make sure the renderer is no longer using this PSO or its resources.
        Logger::get().info(fmt::format(
            "waiting for the GPU to finish work up to this point before destroying a PSO with id \"{}\"",
            getUniquePipelineIdentifier()));
        getRenderer()->waitForGpuToFinishWorkUpToThisPoint();
    }

    std::variant<std::shared_ptr<DirectXPso>, Error> DirectXPso::createGraphicsPso(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros) {
        // Create PSO.
        auto pPso = std::shared_ptr<DirectXPso>(new DirectXPso(
            pRenderer, pPipelineManager, sVertexShaderName, sPixelShaderName, bUsePixelBlending));

        // Generate DirectX PSO.
        auto optionalError = pPso->generateGraphicsPsoForShaders(
            sVertexShaderName,
            sPixelShaderName,
            bUsePixelBlending,
            additionalVertexShaderMacros,
            additionalPixelShaderMacros);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pPso;
    }

    std::optional<Error> DirectXPso::releaseInternalResources() {
        std::scoped_lock resourcesGuard(mtxInternalResources.first);

        if (!mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "PSO was requested to release internal PSO resources but internal resources are already "
                "released, ignoring this request");
            return {};
        }

        // Release graphics PSO.
        auto iNewRefCount = mtxInternalResources.second.pGraphicsPso.Reset();
        if (iNewRefCount != 0) {
            return Error(fmt::format(
                "internal graphics PSO was requested to be released from the "
                "memory but it's still being referenced (new ref count: {}) (PSO ID: {})",
                iNewRefCount,
                getUniquePipelineIdentifier()));
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
        //            return Error(fmt::format(
        //                "internal root signature was requested to be released from the "
        //                "memory but it's still being referenced (new ref count: {}) (PSO ID: {})",
        //                iNewRefCount,
        //                getUniquePsoIdentifier()));
        //        }

#if defined(DEBUG)
        static_assert(
            sizeof(InternalResources) == 104, // NOLINT: current struct size
            "release new resources here");
#endif

        // Done.
        mtxInternalResources.second.bIsReadyForUsage = false;

        return {};
    }

    std::optional<Error> DirectXPso::restoreInternalResources() {
        std::scoped_lock resourcesGuard(mtxInternalResources.first);

        // Recreate internal PSO and root signature.
        auto optionalError = generateGraphicsPsoForShaders(
            getVertexShaderName(),
            getPixelShaderName(),
            isUsingPixelBlending(),
            getAdditionalVertexShaderMacros(),
            getAdditionalPixelShaderMacros());
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return {};
    }

    std::optional<Error> DirectXPso::generateGraphicsPsoForShaders(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros) {
        // Get settings.
        const auto pRenderSettings = getRenderer()->getRenderSettings();
        std::scoped_lock resourcesGuard(mtxInternalResources.first, pRenderSettings->first);

        // Get AA setting.
        const auto msaaState = pRenderSettings->second->getAntialiasingState();
        const auto iMsaaSampleCount = static_cast<int>(msaaState);

        // Make sure the pipeline is not initialized yet.
        if (mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "PSO was requested to generate internal PSO resources but internal resources are already "
                "created, ignoring this request");
            return {};
        }

        // Assign new shaders.
        const bool bVertexShaderNotFound = addShader(sVertexShaderName);
        const bool bPixelShaderNotFound = addShader(sPixelShaderName);

        // Check if shaders were found.
        if (bVertexShaderNotFound || bPixelShaderNotFound) [[unlikely]] {
            return Error(fmt::format(
                "shaders not found in Shader Manager: vertex \"{}\" (found: {}), pixel \"{}\" (found: {})",
                sVertexShaderName,
                !bVertexShaderNotFound,
                sPixelShaderName,
                !bPixelShaderNotFound));
        }

        // Get assigned shader packs.
        const auto pVertexShaderPack = getShader(ShaderType::VERTEX_SHADER).value();
        const auto pPixelShaderPack = getShader(ShaderType::PIXEL_SHADER).value();

        // Get shaders for the current configuration.
        std::set<ShaderMacro> fullVertexShaderConfiguration;
        std::set<ShaderMacro> fullPixelShaderConfiguration;
        auto pVertexShader = std::dynamic_pointer_cast<HlslShader>(
            pVertexShaderPack->getShader(additionalVertexShaderMacros, fullVertexShaderConfiguration));
        auto pPixelShader = std::dynamic_pointer_cast<HlslShader>(
            pPixelShaderPack->getShader(additionalPixelShaderMacros, fullPixelShaderConfiguration));

        // Get DirectX renderer.
        auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(getRenderer());
        if (pDirectXRenderer == nullptr) [[unlikely]] {
            return Error("expected a DirectX renderer");
        }

        // Get vertex shader bytecode and generate its root signature.
        auto shaderBytecode = pVertexShader->getCompiledBlob();
        if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        const ComPtr<IDxcBlob> pVertexShaderBytecode = std::get<ComPtr<IDxcBlob>>(std::move(shaderBytecode));

        // Get pixel shader bytecode and generate its root signature.
        shaderBytecode = pPixelShader->getCompiledBlob();
        if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        const ComPtr<IDxcBlob> pPixelShaderBytecode = std::get<ComPtr<IDxcBlob>>(std::move(shaderBytecode));

        // Generate one root signature from both shaders.
        auto result = RootSignatureGenerator::generate(
            pDirectXRenderer, pDirectXRenderer->getD3dDevice(), pVertexShader.get(), pPixelShader.get());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto generatedRootSignature = std::get<RootSignatureGenerator::Generated>(std::move(result));
        mtxInternalResources.second.pRootSignature = std::move(generatedRootSignature.pRootSignature);
        mtxInternalResources.second.rootParameterIndices =
            std::move(generatedRootSignature.rootParameterIndices);

        // Prepare to create a PSO using these shaders.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // Setup input layout and shaders.
        const auto vInputLayout = pVertexShader->getShaderInputElementDescription();
        psoDesc.InputLayout = {vInputLayout.data(), static_cast<UINT>(vInputLayout.size())};
        psoDesc.pRootSignature = mtxInternalResources.second.pRootSignature.Get();
        psoDesc.VS = {pVertexShaderBytecode->GetBufferPointer(), pVertexShaderBytecode->GetBufferSize()};
        psoDesc.PS = {pPixelShaderBytecode->GetBufferPointer(), pPixelShaderBytecode->GetBufferSize()};

        // Setup rasterizer description.
        CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
        rasterizerDesc.CullMode = bUsePixelBlending ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.MultisampleEnable = static_cast<int>(msaaState != MsaaState::DISABLED);
        psoDesc.RasterizerState = rasterizerDesc;

        // Setup pixel blend description (if needed).
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        if (bUsePixelBlending) {
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

        // Finalize PSO description.
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        if (bUsePixelBlending) {
            // Disable depth writes.
            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DirectXRenderer::getBackBufferFormat();
        psoDesc.SampleDesc.Count = iMsaaSampleCount;
        psoDesc.SampleDesc.Quality =
            msaaState != MsaaState::DISABLED ? (pDirectXRenderer->getMsaaQualityLevel() - 1) : 0;
        psoDesc.DSVFormat = DirectXRenderer::getDepthStencilBufferFormat();

        // Create PSO.
        HRESULT hResult = pDirectXRenderer->getD3dDevice()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(mtxInternalResources.second.pGraphicsPso.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Done.
        mtxInternalResources.second.bIsReadyForUsage = true;
        saveUsedShaderConfiguration(ShaderType::VERTEX_SHADER, std::move(fullVertexShaderConfiguration));
        saveUsedShaderConfiguration(ShaderType::PIXEL_SHADER, std::move(fullPixelShaderConfiguration));

        return {};
    } // namespace ne
} // namespace ne
