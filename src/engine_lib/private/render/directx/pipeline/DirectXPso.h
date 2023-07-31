#pragma once

// Custom.
#include "render/general/pipeline/Pipeline.h"

// External.
#include "directx/d3dx12.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;

    /** DirectX pipeline state object (PSO) wrapper. */
    class DirectXPso : public Pipeline {
    public:
        /** Stores internal resources. */
        struct InternalResources {
            /** Root signature, used in PSO. */
            ComPtr<ID3D12RootSignature> pRootSignature;

            /**
             * Root parameter indices that was used in creation of @ref pRootSignature.
             *
             * Stores pairs of `shader resource name` - `root parameter index`,
             * allows determining what resource is binded to what root parameter index
             * (by using resource name taken from shader file).
             */
            std::unordered_map<std::string, UINT> rootParameterIndices;

            /** Graphics PSO, created using @ref createGraphicsPso. */
            ComPtr<ID3D12PipelineState> pGraphicsPso;

            /** Additional macros to enable for vertex shader configuration. */
            std::set<ShaderMacro> additionalVertexShaderMacros;

            /** Additional macros to enable for pixel shader configuration. */
            std::set<ShaderMacro> additionalPixelShaderMacros;

            // !!! new internal resources go here !!!
            // !!! don't forget to update @ref releaseInternalResources !!!

            /** Whether resources were created or not. */
            bool bIsReadyForUsage = false;
        };

        DirectXPso() = delete;
        DirectXPso(const DirectXPso&) = delete;
        DirectXPso& operator=(const DirectXPso&) = delete;

        virtual ~DirectXPso() override = default;

        /**
         * Assigns vertex and pixel shaders to create a graphics PSO (for usual rendering).
         *
         * @param pRenderer              Parent renderer that owns this pipeline.
         * @param pPipelineManager       Pipeline manager that owns this PSO.
         * @param sVertexShaderName      Name of the compiled vertex shader (see
         * ShaderManager::compileShaders).
         * @param sPixelShaderName       Name of the compiled pixel shader (see
         * ShaderManager::compileShaders).
         * @param bUsePixelBlending      Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param additionalPixelShaderMacros  Additional macros to enable for pixel shader configuration.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        static std::variant<std::shared_ptr<DirectXPso>, Error> createGraphicsPso(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::set<ShaderMacro>& additionalPixelShaderMacros);

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

        /**
         * Returns internal resources that this PSO uses.
         *
         * @return Internal resources.
         */
        inline std::pair<std::recursive_mutex, InternalResources>* getInternalResources() {
            return &mtxInternalResources;
        }

    private:
        /**
         * Constructor.
         *
         * @param pRenderer         Parent renderer that owns this PSO.
         * @param pPipelineManager  Pipeline manager that owns this PSO.
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         */
        DirectXPso(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
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
         * @param sVertexShaderName      Name of the compiled vertex shader (see
         * ShaderManager::compileShaders).
         * @param sPixelShaderName       Name of the compiled pixel shader (see
         * ShaderManager::compileShaders).
         * @param bUsePixelBlending      Whether the PSO should use blending or not (for transparency).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader.
         * @param additionalPixelShaderMacros  Additional macros to enable for pixel shader.
         *
         * @return Error if failed to generate PSO.
         */
        [[nodiscard]] std::optional<Error> generateGraphicsPsoForShaders(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::set<ShaderMacro>& additionalPixelShaderMacros);

        /**
         * Internal resources.
         * Must be used with mutex when changing.
         */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;
    };
} // namespace ne
