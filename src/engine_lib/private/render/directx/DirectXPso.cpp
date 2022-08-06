#include "DirectXPso.h"

// Custom.
#include "render/directx/DirectXRenderer.h"
#include "io/Logger.h"
#include "shaders/hlsl/RootSignatureGenerator.h"

namespace ne {
    DirectXPso::DirectXPso(DirectXRenderer* pRenderer) : ShaderUser(pRenderer->getShaderManager()) {
        this->pRenderer = pRenderer;
    }

    std::optional<Error>
    DirectXPso::setupGraphicsPso(const std::string& sVertexShaderName, const std::string& sPixelShaderName) {
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

        // Generate DirectX PSO.
        auto optionalError = generateGraphicsPsoForShaders();
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return {};
    }

    std::optional<Error> DirectXPso::generateGraphicsPsoForShaders() {
        // Get assigned shader packs.
        const auto pVertexShaderPack = getShader(ShaderType::VERTEX_SHADER).value();
        const auto pPixelShaderPack = getShader(ShaderType::PIXEL_SHADER).value();

        // Prepare lambda to generate error if occurred.
        auto generateErrorMessage = [this](
                                        const std::string& sShaderType,
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
        auto optionalShader =
            pVertexShaderPack->changeConfiguration(pRenderer->getVertexShaderConfiguration());
        if (!optionalShader.has_value()) [[unlikely]] {
            return Error(generateErrorMessage(
                "vertex", pVertexShaderPack->getShaderName(), pRenderer->getVertexShaderConfiguration()));
        }
        const auto pVertexShader = std::dynamic_pointer_cast<HlslShader>(optionalShader.value());

        // Get pixel shader for current configuration.
        optionalShader = pPixelShaderPack->changeConfiguration(pRenderer->getPixelShaderConfiguration());
        if (!optionalShader.has_value()) [[unlikely]] {
            return Error(generateErrorMessage(
                "pixel", pPixelShaderPack->getShaderName(), pRenderer->getPixelShaderConfiguration()));
        }
        const auto pPixelShader = std::dynamic_pointer_cast<HlslShader>(optionalShader.value());

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
        rasterizerDesc.MultisampleEnable = antialiasingSettings.bIsEnabled;

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
