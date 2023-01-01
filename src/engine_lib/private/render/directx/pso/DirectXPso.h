#pragma once

// Custom.
#include "render/pso/Pso.h"

// External.
#include "directx/d3dx12.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;

    /** Our DirectX pipeline state object (PSO) wrapper. */
    class DirectXPso : public Pso {
    public:
        DirectXPso() = delete;
        DirectXPso(const DirectXPso&) = delete;
        DirectXPso& operator=(const DirectXPso&) = delete;

        virtual ~DirectXPso() override = default;

        /**
         * Assigns vertex and pixel shaders to create a graphics PSO (for usual rendering).
         *
         * @param pRenderer Parent renderer that owns this PSO.
         * @param pPsoManager PSO manager that owns this PSO.
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        static std::variant<std::shared_ptr<DirectXPso>, Error> createGraphicsPso(
            Renderer* pRenderer,
            PsoManager* pPsoManager,
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

        /**
         * Releases internal resources such as root signature, internal PSO, etc.
         *
         * @warning Expects that the GPU is not referencing this PSO (command queue is empty) and
         * that no drawing will occur until @ref restoreInternalResources is called.
         *
         * @remark Typically used before changing something (for ex. shader configuration), so that no PSO
         * will reference old resources, to later call @ref restoreInternalResources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> releaseInternalResources() override;

        /**
         * Creates internal resources using the current configuration.
         *
         * @remark Called after @ref releaseInternalResources to create resources that will now reference
         * changed (new) resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> restoreInternalResources() override;

    private:
        /** Stores internal resources. */
        struct InternalDirectXPsoResources {
            /** Root signature, used in PSO. */
            ComPtr<ID3D12RootSignature> pRootSignature;

            /** Graphics PSO, created using @ref createGraphicsPso. */
            ComPtr<ID3D12PipelineState> pGraphicsPso;

            // !!! new internal resources go here !!!
            // !!! don't forget to update @ref releaseInternalResources !!!
        };

        /**
         * Constructor.
         *
         * @param pRenderer Parent renderer that owns this PSO.
         * @param pPsoManager PSO manager that owns this PSO.
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         */
        DirectXPso(
            Renderer* pRenderer,
            PsoManager* pPsoManager,
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

        /**
         * (Re)generates DirectX graphics pipeline state object for the specified shaders.
         *
         * @warning If a shader of some type was already added it will be replaced with the new one.
         * When shader is replaced the old shader gets freed from the memory and
         * a new PSO is immediately generated. Make sure the GPU is not using old shader/PSO.
         *
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the PSO should use blending or not (for transparency).
         *
         * @return Error if failed to generate PSO.
         */
        std::optional<Error> generateGraphicsPsoForShaders(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

        /** Internal resources. */
        InternalDirectXPsoResources internalResources;
    };
} // namespace ne
