#include "DirectXPso.h"

// Custom.
#include "materials/hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#include "materials/hlsl/RootSignatureGenerator.h"

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

    std::optional<Error> DirectXPso::generateGraphicsPsoForShaders(
        const std::string& sVertexShaderName, const std::string& sPixelShaderName, bool bUsePixelBlending) {
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

        // Prepare lambda to generate error if occurred.
        auto generateErrorMessage = [](const std::string& sShaderType,
                                       const std::string& sShaderName,
                                       const std::set<ShaderParameter>& configuration) -> std::string {
            const auto vShaderParameterNames = shaderParametersToText(configuration);
            std::string sShaderConfigurationText;
            if (vShaderParameterNames.empty()) {
                sShaderConfigurationText = "empty configuration";
            } else {
                for (const auto& parameter : vShaderParameterNames) {
                    sShaderConfigurationText += std::format("{} ", parameter);
                }
            }
            return std::format(
                "{} shader pack \"{}\" does not contain a shader that "
                "matches the following shader configuration: {}",
                sShaderType,
                sShaderName,
                sShaderConfigurationText);
        };

        // Get vertex shader for current configuration.
        const auto vertexShaderConfiguration = getRenderer()->getVertexShaderConfiguration();
        auto pShader = pVertexShaderPack->changeConfiguration(vertexShaderConfiguration);
        if (!pShader) [[unlikely]] {
            return Error(generateErrorMessage(
                "vertex", pVertexShaderPack->getShaderName(), vertexShaderConfiguration));
        }
        const auto pVertexShader = std::dynamic_pointer_cast<HlslShader>(pShader);

        // Get pixel shader for current configuration.
        const auto pixelShaderConfiguration = getRenderer()->getPixelShaderConfiguration();
        pShader = pPixelShaderPack->changeConfiguration(pixelShaderConfiguration);
        if (!pShader) [[unlikely]] {
            return Error(
                generateErrorMessage("pixel", pPixelShaderPack->getShaderName(), pixelShaderConfiguration));
        }
        const auto pPixelShader = std::dynamic_pointer_cast<HlslShader>(pShader);

        // Get DirectX renderer.
        DirectXRenderer* pDirectXRenderer = dynamic_cast<DirectXRenderer*>(getRenderer());

        // Generate root signature for both shaders.
        auto result = RootSignatureGenerator::merge(
            pDirectXRenderer->getDevice(), pVertexShader.get(), pPixelShader.get());
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }
        pRootSignature = std::get<ComPtr<ID3D12RootSignature>>(std::move(result));

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

        // Prepare to create PSO from these shaders.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        const auto vInputLayout = pVertexShader->getShaderInputElementDescription();
        psoDesc.InputLayout = {vInputLayout.data(), static_cast<UINT>(vInputLayout.size())};
        psoDesc.pRootSignature = pRootSignature.Get();
        psoDesc.VS = {pVertexShaderBytecode->GetBufferPointer(), pVertexShaderBytecode->GetBufferSize()};
        psoDesc.PS = {pPixelShaderBytecode->GetBufferPointer(), pPixelShaderBytecode->GetBufferSize()};

        const auto antialiasingSettings = getRenderer()->getAntialiasing();

        CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
        rasterizerDesc.CullMode = bUsePixelBlending ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.MultisampleEnable = static_cast<int>(antialiasingSettings.bIsEnabled);

        psoDesc.RasterizerState = rasterizerDesc;
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
                static_cast<int>(getRenderer()->getAntialiasing().bIsEnabled);
        } else {
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        }
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = pDirectXRenderer->getBackBufferFormat();
        psoDesc.SampleDesc.Count = antialiasingSettings.bIsEnabled ? antialiasingSettings.iSampleCount : 1;
        psoDesc.SampleDesc.Quality =
            antialiasingSettings.bIsEnabled ? (pDirectXRenderer->getMsaaQualityLevel() - 1) : 0;
        psoDesc.DSVFormat = pDirectXRenderer->getDepthStencilBufferFormat();

        // Create PSO.
        HRESULT hResult = pDirectXRenderer->getDevice()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(pGraphicsPso.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    } // namespace ne
} // namespace ne
