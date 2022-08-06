#pragma once

// Custom.
#include "shaders/hlsl/HlslShader.h"
#include "shaders/ShaderUser.h"

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;

    /** Our DirectX pipeline state object (PSO) wrapper. */
    class DirectXPso : public ShaderUser {
    public:
        /**
         * Constructor.
         *
         * @param pRenderer Parent renderer that owns this PSO.
         */
        DirectXPso(DirectXRenderer* pRenderer);
        DirectXPso() = delete;
        DirectXPso(const DirectXPso&) = delete;
        DirectXPso& operator=(const DirectXPso&) = delete;

        virtual ~DirectXPso() override = default;

        /**
         * Assigns vertex and pixel shaders to create a graphics PSO (for usual rendering).
         *
         * @warning If a shader of some type was already added it will be replaced with the new one.
         * When shader is replaced the old shader gets freed from the memory and
         * a new PSO is immediately generated. Make sure the GPU is not using old shader/PSO.
         *
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate PSO.
         */
        std::optional<Error>
        setupGraphicsPso(const std::string& sVertexShaderName, const std::string& sPixelShaderName);

    private:
        /**
         * (Re)generates DirectX graphics pipeline state object for assigned shaders.
         * Called by @ref setupGraphicsPso.
         *
         * @warning This function assumes that vertex and pixel shaders are already assigned.
         *
         * @return Error if unable to generate PSO.
         */
        std::optional<Error> generateGraphicsPsoForShaders();

        /** Do not delete. Parent renderer that uses this PSO. */
        DirectXRenderer* pRenderer;

        /** Root signature, used in PSO. */
        ComPtr<ID3D12RootSignature> pRootSignature;

        /** Graphics PSO, created using @ref setupGraphicsPso. */
        ComPtr<ID3D12PipelineState> pGraphicsPso;
    };
} // namespace ne
