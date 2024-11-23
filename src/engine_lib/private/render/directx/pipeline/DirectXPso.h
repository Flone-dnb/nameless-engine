#pragma once

// Custom.
#include "render/general/pipeline/Pipeline.h"
#include "shader/hlsl/SpecialRootParameterSlot.hpp"
#include "render/general/resource/frame/FrameResourceManager.h"
#include "render/directx/resource/DirectXResource.h"

// External.
#include "directx/d3dx12.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXResource;
    class DirectXRenderer;

    /** DirectX pipeline state object (PSO) wrapper. */
    class DirectXPso : public Pipeline {
        // Renderer will ask us to bind global shader resource views.
        friend class DirectXRenderer;

    public:
        /** Stores internal resources. */
        struct InternalResources {
            /** Root signature, used in PSO. */
            ComPtr<ID3D12RootSignature> pRootSignature;

            /** Created PSO. */
            ComPtr<ID3D12PipelineState> pPso;

            /**
             * Root parameter indices that was used in creation of @ref pRootSignature.
             *
             * Stores pairs of `shader resource name` - `root parameter index`,
             * allows determining which resource is binded to which root parameter index
             * (by using resource name taken from shader file).
             */
            std::unordered_map<std::string, UINT> rootParameterIndices;

            /**
             * Stores indices of some non-user specified root parameters. Duplicates some root parameters
             * and their indices from @ref rootParameterIndices but only stored some special non-user
             * specified root parameter indices.
             *
             * @remark Generally used for fast access (without doing a `find` in the map) to some
             * root parameter indices.
             *
             * @remark Example usage: `iRootParameterIndex = vIndices[Slot::FRAME_DATA]`.
             */
            std::array<UINT, static_cast<unsigned int>(SpecialRootParameterSlot::SIZE)>
                vSpecialRootParameterIndices;

            /**
             * Global bindings that should be bound as CBVs. Stores pairs of "root parameter index" -
             * "resource to bind".
             *
             * @remark It's safe to store raw pointers to resources here because the resources
             * must be valid white they are used in the pipeline (so when a pipeline is no longer used it's
             * destroyed and thus this array will be empty) but when the pipeline recreates its internal
             * resources to apply some changes it clears this array and expects the resources to be
             * rebound.
             */
            std::unordered_map<
                UINT,
                std::array<DirectXResource*, FrameResourceManager::getFrameResourceCount()>>
                globalShaderResourceCbvs;

            /**
             * Global bindings that should be bound as SRVs. Stores pairs of "root parameter index" -
             * "resource to bind".
             *
             * @remark See @ref globalShaderResourceCbvs on why it's safe to store raw pointers here.
             */
            std::unordered_map<
                UINT,
                std::array<DirectXResource*, FrameResourceManager::getFrameResourceCount()>>
                globalShaderResourceSrvs;

            /**
             * Stores pairs of "root parameter index" - "descriptor range to bind".
             *
             * @remark Shader resources modify this map.
             */
            std::unordered_map<UINT, std::shared_ptr<ContinuousDirectXDescriptorRange>>
                descriptorRangesToBind;

            /** Whether fields of this struct are initialized or not. */
            bool bIsReadyForUsage = false;
        };

        DirectXPso() = delete;
        DirectXPso(const DirectXPso&) = delete;
        DirectXPso& operator=(const DirectXPso&) = delete;

        virtual ~DirectXPso() override;

        /**
         * Assigns vertex and pixel shaders to create a graphics PSO (for usual rendering).
         *
         * @param pRenderer              Used renderer.
         * @param pPipelineManager       Pipeline manager that owns this PSO.
         * @param pPipelineConfiguration Settings that determine pipeline usage and usage details.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        static std::variant<std::shared_ptr<DirectXPso>, Error> createGraphicsPso(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration);

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
         * Looks for a root parameter that is used for a shader resource with the specified name.
         *
         * @param sShaderResourceName Shader resource name (from the shader code) to look for.
         *
         * @return Error if something went wrong, otherwise root parameter index of the resource with
         * the specified name.
         */
        std::variant<unsigned int, Error> getRootParameterIndex(const std::string& sShaderResourceName);

        /**
         * Returns internal resources that this PSO uses.
         *
         * @return Internal resources.
         */
        std::pair<std::recursive_mutex, InternalResources>* getInternalResources() {
            return &mtxInternalResources;
        }

    protected:
        /**
         * Releases all internal resources from this graphics pipeline and then recreates
         * them to reference new resources/parameters from the renderer.
         *
         * @warning Expects that the GPU is not processing any frames and the rendering is paused
         * (new frames are not submitted) while this function is being called.
         *
         * @remark This function is used when all graphics pipelines reference old render
         * resources/parameters to reference the new (changed) render resources/parameters. The typical
         * workflow goes like this: pause the rendering, change renderer's resource/parameter that all
         * graphics pipelines reference (like render target type (MSAA or not) or MSAA sample count), then
         * call this function (all graphics pipelines will now query up-to-date rendering
         * resources/parameters) and then you can continue rendering.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> recreateInternalResources() override;

    private:
        /**
         * Constructs uninitialized pipeline.
         *
         * @param pRenderer              Used renderer.
         * @param pPipelineManager       Pipeline manager that owns this PSO.
         * @param pPipelineConfiguration Settings and usage details.
         */
        explicit DirectXPso(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration);

        /**
         * Sets views of global shader resource bindings.
         *
         * @warning Expects that the pipeline's internal resources mutex is already locked by the caller.
         *
         * @param pCommandList               Command list to set views to.
         * @param iCurrentFrameResourceIndex Index of the frame resource that is currently being used to
         * submit a new frame.
         */
        void bindGlobalShaderResourceViews(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList, size_t iCurrentFrameResourceIndex) const {
            // No need to lock internal resources mutex since the caller is expected to lock that mutex
            // because this function is expected to be called inside of the `draw` function.

            // Bind global CBVs.
            for (const auto& [iRootParameterIndex, vResourcesToBind] :
                 mtxInternalResources.second.globalShaderResourceCbvs) {
                pCommandList->SetGraphicsRootConstantBufferView(
                    iRootParameterIndex,
                    vResourcesToBind[iCurrentFrameResourceIndex]
                        ->getInternalResource()
                        ->GetGPUVirtualAddress());
            }

            // Bind global SRVs.
            for (const auto& [iRootParameterIndex, vResourcesToBind] :
                 mtxInternalResources.second.globalShaderResourceSrvs) {
                // Set view.
                pCommandList->SetGraphicsRootShaderResourceView(
                    iRootParameterIndex,
                    vResourcesToBind[iCurrentFrameResourceIndex]
                        ->getInternalResource()
                        ->GetGPUVirtualAddress());
            }
        }

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
