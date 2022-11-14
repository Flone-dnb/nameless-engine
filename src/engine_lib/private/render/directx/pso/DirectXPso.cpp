#include "DirectXPso.h"

// Custom.
#include "shaders/hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#include "shaders/hlsl/RootSignatureGenerator.h"

namespace ne {
    DirectXPso::DirectXPso(DirectXRenderer* pRenderer) : ShaderUser(pRenderer->getShaderManager()) {
        this->pRenderer = pRenderer;
    }

    std::variant<std::unique_ptr<DirectXPso>, Error> DirectXPso::createGraphicsPso(
        DirectXRenderer* pRenderer,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName) {
        // Create PSO.
        auto pPso = std::unique_ptr<DirectXPso>(new DirectXPso(pRenderer));

        // Generate DirectX PSO.
        auto optionalError = pPso->generateGraphicsPsoForShaders(sVertexShaderName, sPixelShaderName);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return {};
    }

    std::optional<Error> DirectXPso::generateGraphicsPsoForShaders(
        const std::string& sVertexShaderName, const std::string& sPixelShaderName) {
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
        const auto vertexShaderConfiguration = pRenderer->getVertexShaderConfiguration();
        auto pShader = pVertexShaderPack->changeConfiguration(vertexShaderConfiguration);
        if (!pShader) [[unlikely]] {
            return Error(generateErrorMessage(
                "vertex", pVertexShaderPack->getShaderName(), vertexShaderConfiguration));
        }
        const auto pVertexShader = std::dynamic_pointer_cast<HlslShader>(pShader);

        // Get pixel shader for current configuration.
        const auto pixelShaderConfiguration = pRenderer->getPixelShaderConfiguration();
        pShader = pPixelShaderPack->changeConfiguration(pixelShaderConfiguration);
        if (!pShader) [[unlikely]] {
            return Error(
                generateErrorMessage("pixel", pPixelShaderPack->getShaderName(), pixelShaderConfiguration));
        }
        const auto pPixelShader = std::dynamic_pointer_cast<HlslShader>(pShader);

        // Generate root signature for both shaders.
        auto result =
            RootSignatureGenerator::merge(pRenderer->getDevice(), pVertexShader.get(), pPixelShader.get());
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

        const auto antialiasingSettings = pRenderer->getAntialiasing();

        CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.MultisampleEnable = static_cast<int>(antialiasingSettings.bIsEnabled);

        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = pRenderer->getBackBufferFormat();
        psoDesc.SampleDesc.Count = antialiasingSettings.bIsEnabled ? antialiasingSettings.iSampleCount : 1;
        psoDesc.SampleDesc.Quality =
            antialiasingSettings.bIsEnabled ? (pRenderer->getMsaaQualityLevel() - 1) : 0;
        psoDesc.DSVFormat = pRenderer->getDepthStencilBufferFormat();

        // Create PSO.
        HRESULT hResult = pRenderer->getDevice()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(pGraphicsPso.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    } // namespace ne
} // namespace ne
