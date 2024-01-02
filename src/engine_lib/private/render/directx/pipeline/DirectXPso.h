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

            /** Created PSO. */
            ComPtr<ID3D12PipelineState> pPso;

            // !!! new internal resources go here !!!
            // !!! don't forget to update @ref releaseInternalResources !!!

            /** Whether resources were created or not. */
            bool bIsReadyForUsage = false;
        };

        DirectXPso() = delete;
        DirectXPso(const DirectXPso&) = delete;
        DirectXPso& operator=(const DirectXPso&) = delete;

        virtual ~DirectXPso() override;

        /**
         * Assigns vertex and pixel shaders to create a graphics PSO (for usual rendering).
         *
         * @param pRenderer                 Used renderer.
         * @param pPipelineManager          Pipeline manager that owns this PSO.
         * @param pPipelineCreationSettings Settings that determine pipeline usage and usage details.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        static std::variant<std::shared_ptr<DirectXPso>, Error> createGraphicsPso(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            std::unique_ptr<PipelineCreationSettings> pPipelineCreationSettings);

        /**
         * Assigns compute shader to create a compute PSO.
         *
         * @param pRenderer          Used renderer.
         * @param pPipelineManager   Pipeline manager that owns this PSO.
         * @param sComputeShaderName Name of the compiled compute shader (see ShaderManager::compileShaders).
         *
         * @return Error if shader was not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        static std::variant<std::shared_ptr<DirectXPso>, Error> createComputePso(
            Renderer* pRenderer, PipelineManager* pPipelineManager, const std::string& sComputeShaderName);

        /**
         * Returns internal resources that this PSO uses.
         *
         * @return Internal resources.
         */
        inline std::pair<std::recursive_mutex, InternalResources>* getInternalResources() {
            return &mtxInternalResources;
        }

    protected:
        /**
         * Releases internal resources such as root signature, internal PSO, etc.
         *
         * @warning Expects that the GPU is not referencing this PSO and
         * that no drawing will occur until @ref restoreInternalResources is called.
         *
         * @remark Typically used before changing something (for ex. shader configuration), so that no PSO
         * will reference old resources, to later call @ref restoreInternalResources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> releaseInternalResources() override;

        /**
         * Creates internal resources using the current shader configuration.
         *
         * @remark Called after @ref releaseInternalResources to create resources that will now reference
         * changed (new) resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> restoreInternalResources() override;

    private:
        /**
         * Constructs uninitialized pipeline.
         *
         * @param pRenderer          Used renderer.
         * @param pPipelineManager   Pipeline manager that owns this PSO.
         * @param sVertexShaderName  Name of the compiled vertex shader to use (empty if not used).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param sPixelShaderName   Name of the compiled pixel shader to use (empty if not used).
         * @param additionalPixelShaderMacros Additional macros to enable for pixel shader configuration.
         * @param sComputeShaderName Name of the compiled compute shader to use (empty if not used).
         * @param bEnableDepthBias   Whether depth bias (offset) is enabled or not.
         * @param bUsePixelBlending  Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         */
        DirectXPso(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            const std::string& sVertexShaderName,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::string& sPixelShaderName,
            const std::set<ShaderMacro>& additionalPixelShaderMacros = {},
            const std::string& sComputeShaderName = "",
            bool bEnableDepthBias = false,
            bool bUsePixelBlending = false);

        /**
         * (Re)generates DirectX graphics pipeline state object.
         *
         * @warning If a shader of some type was already added it will be replaced with the new one.
         * When shader is replaced the old shader gets freed from the memory and
         * a new PSO is immediately generated. Make sure the GPU is not using old shader/PSO.
         *
         * @return Error if failed to generate PSO.
         */
        [[nodiscard]] std::optional<Error> generateGraphicsPso();

        /**
         * (Re)generates DirectX compute pipeline state object.
         *
         * @warning If a shader of some type was already added it will be replaced with the new one.
         * When shader is replaced the old shader gets freed from the memory and
         * a new PSO is immediately generated. Make sure the GPU is not using old shader/PSO.
         *
         * @return Error if failed to generate PSO.
         */
        [[nodiscard]] std::optional<Error> generateComputePso();

        /**
         * Internal resources.
         * Must be used with mutex when changing.
         */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;
    };
} // namespace ne
