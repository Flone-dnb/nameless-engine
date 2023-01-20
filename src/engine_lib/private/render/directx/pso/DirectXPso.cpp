#include "DirectXPso.h"

// Custom.
#include "materials/hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#include "materials/hlsl/RootSignatureGenerator.h"
#include "io/Logger.h"
#include "render/RenderSettings.h"

namespace ne {
    DirectXPso::DirectXPso(
        Renderer* pRenderer,
        PsoManager* pPsoManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending)
        : Pso(pRenderer, pPsoManager, sVertexShaderName, sPixelShaderName, bUsePixelBlending) {}

    std::variant<std::shared_ptr<DirectXPso>, Error> DirectXPso::createGraphicsPso(
        Renderer* pRenderer,
        PsoManager* pPsoManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending) {
        // Create PSO.
        auto pPso = std::shared_ptr<DirectXPso>(
            new DirectXPso(pRenderer, pPsoManager, sVertexShaderName, sPixelShaderName, bUsePixelBlending));

        // Generate DirectX PSO.
        auto optionalError =
            pPso->generateGraphicsPsoForShaders(sVertexShaderName, sPixelShaderName, bUsePixelBlending);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pPso;
    }

    std::optional<Error> DirectXPso::releaseInternalResources() {
        std::scoped_lock resourcesGuard(mtxInternalResources.first);

        if (!mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "PSO was requested to release internal PSO resources but internal resources are already "
                "released, ignoring this request",
                sDirectXPsoLogCategory);
            return {};
        }

        // Release graphics PSO.
        auto iNewRefCount = mtxInternalResources.second.pGraphicsPso.Reset();
        if (iNewRefCount != 0) {
            return Error(std::format(
                "internal graphics PSO was requested to be released from the "
                "memory but it's still being referenced (new ref count: {}) (PSO ID: {})",
                iNewRefCount,
                getUniquePsoIdentifier()));
        }

        // Release root signature.
        iNewRefCount = mtxInternalResources.second.pRootSignature.Reset();
        if (iNewRefCount != 0) {
            return Error(std::format(
                "internal root signature was requested to be released from the "
                "memory but it's still being referenced (new ref count: {}) (PSO ID: {})",
                iNewRefCount,
                getUniquePsoIdentifier()));
        }

        // !!!
        // !!! new resources go here !!!
        // !!!

        // Done.
        mtxInternalResources.second.bIsReadyForUsage = false;

        return {};
    }

    std::optional<Error> DirectXPso::restoreInternalResources() {
        // Recreate internal PSO and root signature.
        auto optionalError = generateGraphicsPsoForShaders(
            getVertexShaderName(), getPixelShaderName(), isUsingPixelBlending());
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        return {};
    }

    std::optional<Error> DirectXPso::generateGraphicsPsoForShaders(
        const std::string& sVertexShaderName, const std::string& sPixelShaderName, bool bUsePixelBlending) {
        // Get settings.
        const auto pRenderSettings = getRenderer()->getRenderSettings();

        std::scoped_lock resourcesGuard(mtxInternalResources.first, pRenderSettings->first);

        if (mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "PSO was requested to generate internal PSO resources but internal resources are already "
                "created, ignoring this request",
                sDirectXPsoLogCategory);
            return {};
        }

        // Assign new shaders.
        const bool bVertexShaderNotFound = addShader(sVertexShaderName);
        const bool bPixelShaderNotFound = addShader(sPixelShaderName);

        // Check if shaders were found.
        if (bVertexShaderNotFound || bPixelShaderNotFound) {
            return Error(std::format(
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
        const auto pVertexShader = std::dynamic_pointer_cast<HlslShader>(pVertexShaderPack->getShader());
        const auto pPixelShader = std::dynamic_pointer_cast<HlslShader>(pPixelShaderPack->getShader());

        // Get DirectX renderer.
        DirectXRenderer* pDirectXRenderer = dynamic_cast<DirectXRenderer*>(getRenderer());
        if (pDirectXRenderer == nullptr) [[unlikely]] {
            Error error("DirectX pipeline state object is used with non-DirectX renderer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Generate one root signature for both shaders.
        auto result = RootSignatureGenerator::merge(
            pDirectXRenderer->getDevice(), pVertexShader.get(), pPixelShader.get());
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }
        mtxInternalResources.second.pRootSignature = std::get<ComPtr<ID3D12RootSignature>>(std::move(result));

        // Get vertex shader bytecode.
        auto shaderBytecode = pVertexShader->getCompiledBlob();
        if (std::holds_alternative<Error>(shaderBytecode)) {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addEntry();
            return err;
        }
        const ComPtr<IDxcBlob> pVertexShaderBytecode = std::get<ComPtr<IDxcBlob>>(std::move(shaderBytecode));

        // Get pixel shader bytecode.
        shaderBytecode = pPixelShader->getCompiledBlob();
        if (std::holds_alternative<Error>(shaderBytecode)) {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addEntry();
            return err;
        }
        const ComPtr<IDxcBlob> pPixelShaderBytecode = std::get<ComPtr<IDxcBlob>>(std::move(shaderBytecode));

        // Prepare to create a PSO from these shaders.
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
        rasterizerDesc.MultisampleEnable = static_cast<int>(pRenderSettings->second->isAntialiasingEnabled());
        psoDesc.RasterizerState = rasterizerDesc;

        // Setup pixel blend description (if needed).
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
            psoDesc.BlendState.AlphaToCoverageEnable =
                static_cast<int>(pRenderSettings->second->isAntialiasingEnabled());
        } else {
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        }

        // Finalize PSO description.
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = pDirectXRenderer->getBackBufferFormat();
        psoDesc.SampleDesc.Count =
            pRenderSettings->second->isAntialiasingEnabled()
                ? static_cast<unsigned int>(pRenderSettings->second->getAntialiasingQuality())
                : 1;
        psoDesc.SampleDesc.Quality = pRenderSettings->second->isAntialiasingEnabled()
                                         ? (pDirectXRenderer->getMsaaQualityLevel() - 1)
                                         : 0;
        psoDesc.DSVFormat = pDirectXRenderer->getDepthStencilBufferFormat();

        // Create PSO.
        HRESULT hResult = pDirectXRenderer->getDevice()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(mtxInternalResources.second.pGraphicsPso.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Done.
        mtxInternalResources.second.bIsReadyForUsage = true;

        return {};
    } // namespace ne
} // namespace ne
