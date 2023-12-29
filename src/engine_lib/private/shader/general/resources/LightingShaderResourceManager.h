#pragma once

// Standard.
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>

// Custom.
#include "render/general/resources/UploadBuffer.h"
#include "render/general/resources/frame/FrameResourcesManager.h"
#if defined(WIN32)
#include "render/directx/resources/DirectXResource.h"
#include "shader/hlsl/RootSignatureGenerator.h"
#include "render/directx/pipeline/DirectXPso.h"
#endif
#include "shader/general/resources/ShaderLightArray.h"

// External.
#include "vulkan/vulkan_core.h"
#if defined(WIN32)
#include "directx/d3dx12.h"
#include <wrl.h>
#endif

namespace ne {
#if defined(WIN32)
    using namespace Microsoft::WRL;
#endif

    class Renderer;
    class Pipeline;
    class ComputeShaderInterface;
    class VulkanPipeline;

    /**
     * Manages GPU resources that store lighting related data (such as data of all spawned light sources
     * (data such as color, intensity, position, etc.)).
     */
    class LightingShaderResourceManager {
        // Only renderer is allowed to create and update resources of this manager.
        friend class Renderer;

    public:
        /** Data that will be directly copied into shaders. */
        struct GeneralLightingShaderData {
            /** Light color intensity of ambient lighting. 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 ambientLight = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);

            /** Total number of spawned point lights in camera frustum. */
            alignas(iVkScalarAlignment) unsigned int iPointLightCountInCameraFrustum = 0;

            /** Total number of spawned directional lights. */
            alignas(iVkScalarAlignment) unsigned int iDirectionalLightCount = 0;

            /** Total number of spawned spotlights in camera frustum. */
            alignas(iVkScalarAlignment) unsigned int iSpotLightCountInCameraFrustum = 0;
        };

        /** Groups GPU related data. */
        struct GpuData {
            /** Stores data from @ref generalData in the GPU memory. */
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vGeneralDataGpuResources;

            /**
             * Stores general (not related to a specific light source type) lighting data.
             *
             * @remark Stores data that is copied in @ref vGeneralDataGpuResources.
             */
            GeneralLightingShaderData generalData;
        };

        LightingShaderResourceManager() = delete;

        ~LightingShaderResourceManager();

        /**
         * Return name of the shader resource that stores general lighting data (name from shader code).
         *
         * @return Name of the shader resource.
         */
        static std::string getGeneralLightingDataShaderResourceName();

        /**
         * Return name of the shader resource that stores array of point lights (name from shader code).
         *
         * @return Name of the shader resource.
         */
        static std::string getPointLightsShaderResourceName();

        /**
         * Return name of the shader resource that stores array of directional lights (name from shader code).
         *
         * @return Name of the shader resource.
         */
        static std::string getDirectionalLightsShaderResourceName();

        /**
         * Return name of the shader resource that stores array of spotlights (name from shader code).
         *
         * @return Name of the shader resource.
         */
        static std::string getSpotlightsShaderResourceName();

        /**
         * Return name of the shader resource that stores indices of point lights in camera's frustum (name
         * from shader code).
         *
         * @return Name of the shader resource.
         */
        static std::string getPointLightsInCameraFrustumIndicesShaderResourceName();

        /**
         * Return name of the shader resource that stores indices of spotlights in camera's frustum (name
         * from shader code).
         *
         * @return Name of the shader resource.
         */
        static std::string getSpotlightsInCameraFrustumIndicesShaderResourceName();

#if defined(WIN32)
        /**
         * Sets resource view of the specified lighting array to the specified command list.
         *
         * @warning Expects that the specified pipeline's shaders indeed uses lighting resources.
         *
         * @warning Expects that the specified pipeline's internal resources mutex is locked.
         *
         * @param pPso                       In debug builds used to make sure the resource is actually used.
         * @param pCommandList               Command list to set view to.
         * @param iCurrentFrameResourceIndex Index of the frame resource that is currently being used to
         * submit a new frame.
         * @param pArray                     Lighting array to bind.
         * @param sArrayNameInShaders        Name of the shader resources used for this lighting array.
         * @param iArrayRootParameterIndex   Index of this resource in the root signature.
         */
        static inline void setLightingArrayViewToCommandList(
            DirectXPso* pPso,
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList,
            size_t iCurrentFrameResourceIndex,
            const std::unique_ptr<ShaderLightArray>& pArray,
            const std::string& sArrayNameInShaders,
            UINT iArrayRootParameterIndex) {
            std::scoped_lock lightsGuard(pArray->mtxResources.first);

#if defined(DEBUG)
            // Self check: make sure this resource is actually being used in the PSO.
            if (pPso->getInternalResources()->second.rootParameterIndices.find(sArrayNameInShaders) ==
                pPso->getInternalResources()->second.rootParameterIndices.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" is not used in the shaders of the specified PSO \"{}\" "
                    "but you are attempting to set this resource to a command list",
                    sArrayNameInShaders,
                    pPso->getPipelineIdentifier()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Bind lights array
            // (resource is guaranteed to be not `nullptr`, see field docs).
            pCommandList->SetGraphicsRootShaderResourceView(
                iArrayRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    pArray->mtxResources.second.vGpuArrayLightDataResources[iCurrentFrameResourceIndex]
                        ->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());
        }
#endif

        /**
         * Returns a non-owning reference to an array that stores data of all spawned point lights.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Array that stores data of all spawned point lights.
         */
        ShaderLightArray* getPointLightDataArray() const;

        /**
         * Returns a non-owning reference to an array that stores data of all spawned directional lights.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Array that stores data of all spawned directional lights.
         */
        ShaderLightArray* getDirectionalLightDataArray() const;

        /**
         * Returns a non-owning reference to an array that stores data of all spawned spotlights.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Array that stores data of all spawned spotlights.
         */
        ShaderLightArray* getSpotlightDataArray() const;

        /**
         * Updates descriptors in all graphics pipelines to make descriptors reference the underlying
         * buffers.
         *
         * @remark Does nothing if DirectX renderer is used.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindDescriptorsToRecreatedPipelineResources();

        /**
         * Updates descriptors in the specified graphics pipeline to make descriptors reference the underlying
         * buffers.
         *
         * @remark Does nothing if DirectX renderer is used.
         *
         * @param pPipeline Pipeline to get descriptors from.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateDescriptorsForPipelineResource(Pipeline* pPipeline);

        /**
         * Returns manager's internal resources.
         *
         * @remark Generally used for tests (read-only), you should not modify them.
         *
         * @return Internal resources.
         */
        std::pair<std::recursive_mutex, GpuData>* getInternalResources();

#if defined(WIN32)
        /**
         * Sets views (CBV/SRV) of lighting resources to the specified command list.
         *
         * @warning Expects that the specified pipeline's shaders indeed use lighting resources.
         *
         * @warning Expects that the specified pipeline's internal resources mutex is locked.
         *
         * @param pPso                       PSO to query for root signature index of the lighting resources.
         * @param pCommandList               Command list to set views to.
         * @param iCurrentFrameResourceIndex Index of the frame resource that is currently being used to
         * submit a new frame.
         */
        inline void setResourceViewToCommandList(
            DirectXPso* pPso,
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList,
            size_t iCurrentFrameResourceIndex) {
            // Lock internal resources.
            std::scoped_lock gpuDataGuard(mtxGpuData.first);

#if defined(DEBUG)
            // Self check: make sure this resource is actually being used in the PSO.
            if (pPso->getInternalResources()->second.rootParameterIndices.find(
                    sGeneralLightingDataShaderResourceName) ==
                pPso->getInternalResources()->second.rootParameterIndices.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" is not used in the shaders of the specified PSO \"{}\" "
                    "but you are attempting to set this resource to a command list",
                    sGeneralLightingDataShaderResourceName,
                    pPso->getPipelineIdentifier()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Bind general lighting resources buffer.
            pCommandList->SetGraphicsRootConstantBufferView(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iGeneralLightingConstantBufferRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    mtxGpuData.second.vGeneralDataGpuResources[iCurrentFrameResourceIndex]
                        ->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind point lights array.
            setLightingArrayViewToCommandList(
                pPso,
                pCommandList,
                iCurrentFrameResourceIndex,
                lightArrays.pPointLightDataArray,
                sPointLightsShaderResourceName,
                RootSignatureGenerator::ConstantRootParameterIndices::iPointLightsBufferRootParameterIndex);

            // Bind directional lights array.
            setLightingArrayViewToCommandList(
                pPso,
                pCommandList,
                iCurrentFrameResourceIndex,
                lightArrays.pDirectionalLightDataArray,
                sDirectionalLightsShaderResourceName,
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iDirectionalLightsBufferRootParameterIndex);

            // Bind spotlights array.
            setLightingArrayViewToCommandList(
                pPso,
                pCommandList,
                iCurrentFrameResourceIndex,
                lightArrays.pSpotlightDataArray,
                sSpotlightsShaderResourceName,
                RootSignatureGenerator::ConstantRootParameterIndices::iSpotlightsBufferRootParameterIndex);

#if defined(DEBUG)
            static_assert(sizeof(LightArrays) == 24, "consider adding new arrays here"); // NOLINT
#endif
        }

        /**
         * Sets views (CBV/SRV) of light grids and light index lists for opaque geometry to the specified
         * command list.
         *
         * @warning Expects that the specified pipeline's shaders indeed use lighting resources.
         *
         * @warning Expects that the specified pipeline's internal resources mutex is locked.
         *
         * @param pPso                       PSO to query for root signature index of the lighting resources.
         * @param pCommandList               Command list to set views to.
         * @param iCurrentFrameResourceIndex Index of the frame resource that is currently being used to
         * submit a new frame.
         */
        inline void setOpaqueLightGridResourcesViewToCommandList(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList) const {
            // Bind point light index list.
            pCommandList->SetGraphicsRootUnorderedAccessView(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingPointLightIndexLightRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    lightCullingComputeShaderData.resources.pOpaquePointLightIndexList.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind spotlight index list.
            pCommandList->SetGraphicsRootUnorderedAccessView(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingSpotlightIndexLightRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    lightCullingComputeShaderData.resources.pOpaqueSpotLightIndexList.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind directional light index list.
            pCommandList->SetGraphicsRootUnorderedAccessView(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingDirectionalLightIndexLightRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    lightCullingComputeShaderData.resources.pOpaqueDirectionalLightIndexList.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind point light grid.
            pCommandList->SetGraphicsRootDescriptorTable(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingPointLightGridRootParameterIndex,
                *reinterpret_cast<DirectXResource*>(
                     lightCullingComputeShaderData.resources.pOpaquePointLightGrid.get())
                     ->getBindedDescriptorGpuHandle(DirectXDescriptorType::UAV));

            // Bind spotlight light grid.
            pCommandList->SetGraphicsRootDescriptorTable(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingSpotlightGridRootParameterIndex,
                *reinterpret_cast<DirectXResource*>(
                     lightCullingComputeShaderData.resources.pOpaqueSpotLightGrid.get())
                     ->getBindedDescriptorGpuHandle(DirectXDescriptorType::UAV));

            // Bind directional light grid.
            pCommandList->SetGraphicsRootDescriptorTable(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingDirectionalLightGridRootParameterIndex,
                *reinterpret_cast<DirectXResource*>(
                     lightCullingComputeShaderData.resources.pOpaqueDirectionalLightGrid.get())
                     ->getBindedDescriptorGpuHandle(DirectXDescriptorType::UAV));
        }

        /**
         * Sets views (CBV/SRV) of light grids and light index lists for transparent geometry to the specified
         * command list.
         *
         * @warning Expects that the specified pipeline's shaders indeed use lighting resources.
         *
         * @warning Expects that the specified pipeline's internal resources mutex is locked.
         *
         * @param pPso                       PSO to query for root signature index of the lighting resources.
         * @param pCommandList               Command list to set views to.
         * @param iCurrentFrameResourceIndex Index of the frame resource that is currently being used to
         * submit a new frame.
         */
        inline void setTransparentLightGridResourcesViewToCommandList(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList) const {
            // Bind point light index list.
            pCommandList->SetGraphicsRootUnorderedAccessView(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingPointLightIndexLightRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    lightCullingComputeShaderData.resources.pTransparentPointLightIndexList.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind spotlight index list.
            pCommandList->SetGraphicsRootUnorderedAccessView(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingSpotlightIndexLightRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    lightCullingComputeShaderData.resources.pTransparentSpotLightIndexList.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind directional light index list.
            pCommandList->SetGraphicsRootUnorderedAccessView(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingDirectionalLightIndexLightRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    lightCullingComputeShaderData.resources.pTransparentDirectionalLightIndexList.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind point light grid.
            pCommandList->SetGraphicsRootDescriptorTable(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingPointLightGridRootParameterIndex,
                *reinterpret_cast<DirectXResource*>(
                     lightCullingComputeShaderData.resources.pTransparentPointLightGrid.get())
                     ->getBindedDescriptorGpuHandle(DirectXDescriptorType::UAV));

            // Bind spotlight light grid.
            pCommandList->SetGraphicsRootDescriptorTable(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingSpotlightGridRootParameterIndex,
                *reinterpret_cast<DirectXResource*>(
                     lightCullingComputeShaderData.resources.pTransparentSpotLightGrid.get())
                     ->getBindedDescriptorGpuHandle(DirectXDescriptorType::UAV));

            // Bind directional light grid.
            pCommandList->SetGraphicsRootDescriptorTable(
                RootSignatureGenerator::ConstantRootParameterIndices::
                    iLightCullingDirectionalLightGridRootParameterIndex,
                *reinterpret_cast<DirectXResource*>(
                     lightCullingComputeShaderData.resources.pTransparentDirectionalLightGrid.get())
                     ->getBindedDescriptorGpuHandle(DirectXDescriptorType::UAV));
        }

#endif

    private:
        /** Groups GPU resources that store arrays of light sources. */
        struct LightArrays {
            /** Stores data of all spawned point lights. */
            std::unique_ptr<ShaderLightArray> pPointLightDataArray;

            /** Stores data of all spawned directional lights. */
            std::unique_ptr<ShaderLightArray> pDirectionalLightDataArray;

            /** Stores data of all spawned spotlights. */
            std::unique_ptr<ShaderLightArray> pSpotlightDataArray;
        };

        /** Groups shader data for compute shaders that operate on light-related data. */
        struct ComputeShaderData {
            /**
             * Plane represented by a normal and a distance.
             *
             * @warning Exactly equal to the struct defined in shaders.
             */
            struct Plane {
                /** Plane's normal. */
                glm::vec3 normal;

                /** Distance from the origin to the nearest point on the plane. */
                float distanceFromOrigin;
            };

            /**
             * Frustum in view space.
             *
             * @warning Exactly equal to the struct defined in shaders.
             */
            struct Frustum {
                /** Left, right, top and bottom faces of the frustum. */
                Plane planes[4];
            };

            /** Groups shader data for compute shader that recalculates light grid frustums. */
            struct FrustumGridComputeShader {
                /** Stores some additional information (some information not available as built-in semantics).
                 */
                struct ComputeInfo {
                    /** Total number of thread groups dispatched in the X direction. */
                    alignas(iVkScalarAlignment) unsigned int iThreadGroupCountX = 0;

                    /** Total number of thread groups dispatched in the Y direction. */
                    alignas(iVkScalarAlignment) unsigned int iThreadGroupCountY = 0;

                    /** Total number of tiles in the X direction. */
                    alignas(iVkScalarAlignment) unsigned int iTileCountX = 0;

                    /** Total number of tiles in the Y direction. */
                    alignas(iVkScalarAlignment) unsigned int iTileCountY = 0;
                };

                /** Data that is used to convert coordinates from screen space to view space. */
                struct ScreenToViewData {
                    /** Inverse of the projection matrix. */
                    alignas(iVkMat4Alignment) glm::mat4 inverseProjectionMatrix = glm::identity<glm::mat4>();

                    /** Width of the viewport (might be smaller that the actual screen size). */
                    alignas(iVkScalarAlignment) unsigned int iRenderTargetWidth = 0;

                    /** Height of the viewport (might be smaller that the actual screen size). */
                    alignas(iVkScalarAlignment) unsigned int iRenderTargetHeight = 0;
                };

                /** Groups buffers that we bind to a compute shader. */
                struct ShaderResources {
                    /**
                     * Buffer that stores additional information that might not be available as built-in
                     * semantics.
                     */
                    std::unique_ptr<UploadBuffer> pComputeInfo;

                    /**
                     * Buffer that stores data that is used to convert coordinates from screen space to
                     * view space.
                     */
                    std::unique_ptr<UploadBuffer> pScreenToViewData;

                    /** Buffer that stores calculated grid of frustums (results of a compute shader). */
                    std::unique_ptr<GpuResource> pCalculatedFrustums;
                };

                /** Groups compute interface and its resources. */
                struct ComputeShader {
                    /**
                     * Creates compute interface and resources and binds them to the interface.
                     *
                     * @param pRenderer Renderer.
                     *
                     * @return Error if something went wrong.
                     */
                    [[nodiscard]] std::optional<Error> initialize(Renderer* pRenderer);

                    /**
                     * Updates data used by shaders.
                     *
                     * @param pRenderer               Renderer.
                     * @param renderTargetSize        Size of the underlying render image in pixels.
                     * @param inverseProjectionMatrix Inverse projection matrix of the active camera.
                     *
                     * @return Error if something went wrong.
                     */
                    [[nodiscard]] std::optional<Error> updateDataAndSubmitShader(
                        Renderer* pRenderer,
                        const std::pair<unsigned int, unsigned int>& renderTargetSize,
                        const glm::mat4& inverseProjectionMatrix);

                    /** Shader Compute. */
                    std::unique_ptr<ComputeShaderInterface> pComputeInterface;

                    /** Shader resources. */
                    ShaderResources resources;

                    /**
                     * Total number of tiles in the X direction that was used when @ref
                     * updateDataAndSubmitShader was called the last time.
                     */
                    unsigned int iLastUpdateTileCountX = 0;

                    /**
                     * Total number of tiles in the X direction that was used when @ref
                     * updateDataAndSubmitShader was called the last time.
                     */
                    unsigned int iLastUpdateTileCountY = 0;

                    /** `true` if @ref initialize was called, `false` otherwise. */
                    bool bIsInitialized = false;

                    /**
                     * Name of the shader resource (name from shader code) that stores
                     * additional data for compute shader.
                     */
                    static inline const auto sComputeInfoShaderResourceName = "computeInfo";

                    /**
                     * Name of the shader resource (name from shader code) that stores
                     * data to convert from screen space to view space.
                     */
                    static inline const auto sScreenToViewDataShaderResourceName = "screenToViewData";

                    /**
                     * Name of the shader resource (name from shader code) that stores
                     * calculated frustums.
                     */
                    static inline const auto sCalculatedFrustumsShaderResourceName = "calculatedFrustums";
                };
            };

            /** Groups shader data for compute shader that does light culling. */
            struct LightCullingComputeShader {
                /** Global counters (indices) into the light lists. */
                struct GlobalCountersIntoLightIndexList {
                    /** Index into point light index light for opaque geometry. */
                    alignas(iVkScalarAlignment) unsigned int iPointLightListOpaque = 0;

                    /** Index into spotlight index light for opaque geometry. */
                    alignas(iVkScalarAlignment) unsigned int iSpotlightListOpaque = 0;

                    /** Index into directional light index light for opaque geometry. */
                    alignas(iVkScalarAlignment) unsigned int iDirectionalLightListOpaque = 0;

                    /** Index into point light index light for transparent geometry. */
                    alignas(iVkScalarAlignment) unsigned int iPointLightListTransparent = 0;

                    /** Index into spotlight index light for transparent geometry. */
                    alignas(iVkScalarAlignment) unsigned int iSpotlightListTransparent = 0;

                    /** Index into directional light index light for transparent geometry. */
                    alignas(iVkScalarAlignment) unsigned int iDirectionalLightListTransparent = 0;
                };

                /**
                 * Stores some additional information (some information not available as built-in semantics).
                 */
                struct ThreadGroupCount {
                    /** Total number of thread groups dispatched in the X direction. */
                    alignas(iVkScalarAlignment) unsigned int iThreadGroupCountX = 0;

                    /** Total number of thread groups dispatched in the Y direction. */
                    alignas(iVkScalarAlignment) unsigned int iThreadGroupCountY = 0;
                };

                /** Groups buffers that we bind to a compute shader. */
                struct ShaderResources {
                    /**
                     * Buffer that stores additional information that might not be available as built-in
                     * semantics.
                     */
                    std::unique_ptr<UploadBuffer> pThreadGroupCount;

                    /** Stores global counters into various light index lists. */
                    std::unique_ptr<GpuResource> pGlobalCountersIntoLightIndexList;

                    /** Stores indices into array of point lights for opaque geometry. */
                    std::unique_ptr<GpuResource> pOpaquePointLightIndexList;

                    /** Stores indices into array of spotlights for opaque geometry. */
                    std::unique_ptr<GpuResource> pOpaqueSpotLightIndexList;

                    /** Stores indices into array of directional lights for opaque geometry. */
                    std::unique_ptr<GpuResource> pOpaqueDirectionalLightIndexList;

                    /** Stores indices into array of point lights for transparent geometry. */
                    std::unique_ptr<GpuResource> pTransparentPointLightIndexList;

                    /** Stores indices into array of spotlights for transparent geometry. */
                    std::unique_ptr<GpuResource> pTransparentSpotLightIndexList;

                    /** Stores indices into array of directional lights for transparent geometry. */
                    std::unique_ptr<GpuResource> pTransparentDirectionalLightIndexList;

                    /**
                     * 2D texture where every pixel stores 2 values: offset into light index list and the
                     * number of elements to read from that offset.
                     */
                    std::unique_ptr<GpuResource> pOpaquePointLightGrid;

                    /**
                     * 2D texture where every pixel stores 2 values: offset into light index list and the
                     * number of elements to read from that offset.
                     */
                    std::unique_ptr<GpuResource> pOpaqueSpotLightGrid;

                    /**
                     * 2D texture where every pixel stores 2 values: offset into light index list and the
                     * number of elements to read from that offset.
                     */
                    std::unique_ptr<GpuResource> pOpaqueDirectionalLightGrid;

                    /**
                     * 2D texture where every pixel stores 2 values: offset into light index list and the
                     * number of elements to read from that offset.
                     */
                    std::unique_ptr<GpuResource> pTransparentPointLightGrid;

                    /**
                     * 2D texture where every pixel stores 2 values: offset into light index list and the
                     * number of elements to read from that offset.
                     */
                    std::unique_ptr<GpuResource> pTransparentSpotLightGrid;

                    /**
                     * 2D texture where every pixel stores 2 values: offset into light index list and the
                     * number of elements to read from that offset.
                     */
                    std::unique_ptr<GpuResource> pTransparentDirectionalLightGrid;

                    /**
                     * The number of tiles in the X direction of the light grid that were set the last time we
                     * (re)created light index lists or light grid resources.
                     */
                    size_t iLightGridTileCountX = 0;

                    /**
                     * The number of tiles in the Y direction of the light grid that were set the last time
                     * we (re)created light index lists or light grid resources.
                     */
                    size_t iLightGridTileCountY = 0;
                };

                /** Groups compute interface and its resources. */
                struct ComputeShader {
                    /**
                     * Creates compute interface and resources and binds them to the interface.
                     *
                     * @param pRenderer         Renderer.
                     * @param frustumGridShader Compute shader that calculates grid of frustums for
                     * light culling. Its resources will be used in light culling.
                     *
                     * @return Error if something went wrong.
                     */
                    [[nodiscard]] std::optional<Error> initialize(
                        Renderer* pRenderer,
                        const FrustumGridComputeShader::ComputeShader& frustumGridShader);

                    /**
                     * (Re)creates light index lists and light grid if tile count was changed
                     * and binds them to the shader.
                     *
                     * @remark Does nothing if the tile count is the same as in the previous call to this
                     * function.
                     *
                     * @param pRenderer   Renderer.
                     * @param iTileCountX The current number of light grid tiles in the X direction.
                     * @param iTileCountY The current number of light grid tiles in the Y direction.
                     *
                     * @return `true` if resources were re-created, `false` if not, otherwise error if
                     * something went wrong.
                     */
                    [[nodiscard]] std::variant<bool, Error> createLightIndexListsAndLightGrid(
                        Renderer* pRenderer, size_t iTileCountX, size_t iTileCountY);

                    /**
                     * Called to queue compute shader to be executed on the next frame.
                     *
                     * @warning Expected to be called somewhere inside of the `drawNextFrame` function
                     * so that renderer's depth texture without multisampling pointer will not change.
                     *
                     * @param pRenderer                  Renderer.
                     * @param pCurrentFrameResource      Current frame resource that will be used to
                     * submit the next frame.
                     * @param iCurrentFrameResourceIndex Index of the frame resource that will be used
                     * to submit the next frame.
                     * @param pGeneralLightingData       GPU resource that stores general lighting
                     * information.
                     * @param pPointLightArray           Array that stores all spawned point lights.
                     * @param pSpotlightArray            Array that stores all spawned spotlights.
                     * @param pNonCulledPointLightsIndicesArray Array that stores indices of point lights
                     * in camera's frustum.
                     * @param pNonCulledSpotlightsIndicesArray  Array that stores indices of spotlights in
                     * camera's frustum.
                     *
                     * @return Error if something went wrong.
                     */
                    [[nodiscard]] std::optional<Error> queueExecutionForNextFrame(
                        Renderer* pRenderer,
                        FrameResource* pCurrentFrameResource,
                        size_t iCurrentFrameResourceIndex,
                        GpuResource* pGeneralLightingData,
                        GpuResource* pPointLightArray,
                        GpuResource* pSpotlightArray,
                        GpuResource* pNonCulledPointLightsIndicesArray,
                        GpuResource* pNonCulledSpotlightsIndicesArray);

                    /** Compute interface. */
                    std::unique_ptr<ComputeShaderInterface> pComputeInterface;

                    /** Shader resources that this compute shader "owns". */
                    ShaderResources resources;

                    /** Stores the number of thread groups that we need to dispatch. */
                    ThreadGroupCount threadGroupCount;

                    /** `true` if @ref initialize was called, `false` otherwise. */
                    bool bIsInitialized = false;

                    /**
                     * Name of the shader resource (name from shader code) that stores
                     * depth texture recorded on depth prepass.
                     */
                    static inline const auto sDepthTextureShaderResourceName = "depthTexture";

                    /**
                     * Name of the shader resource (name from shader code) that stores additional information
                     * that might not be available as built-in semantics.
                     */
                    static inline const auto sThreadGroupCountShaderResourceName = "threadGroupCount";

                    /**
                     * Name of the shader resource (name from shader code) that stores global counters
                     * into various light index lists.
                     */
                    static inline const auto sGlobalCountersIntoLightIndexListShaderResourceName =
                        "globalCountersIntoLightIndexList";
                };
            };
        };

        /**
         * Creates a new manager.
         *
         * @param pRenderer Used renderer.
         *
         * @return Created manager.
         */
        static std::unique_ptr<LightingShaderResourceManager> create(Renderer* pRenderer);

        /**
         * Looks if the pipeline uses the specified shader resource and binds the specified
         * buffers to pipeline's descriptors.
         *
         * @param pVulkanPipeline     Pipeline which descriptors to update.
         * @param pLogicalDevice      Renderer's logical device.
         * @param sShaderResourceName Name of the shader resource to look in the pipeline.
         * @param iResourceSize       Size of one (buffer) shader resource.
         * @param descriptorType      Type of descriptor to bind.
         * @param vBuffersToBind      Buffers to bind (one per frame resource).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> rebindBufferResourceToPipeline(
            VulkanPipeline* pVulkanPipeline,
            VkDevice pLogicalDevice,
            const std::string& sShaderResourceName,
            VkDeviceSize iResourceSize,
            VkDescriptorType descriptorType,
            std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vBuffersToBind);

        /**
         * Looks if the pipeline uses the specified shader resource and binds the specified
         * buffers to pipeline's descriptors.
         *
         * @param pVulkanPipeline     Pipeline which descriptors to update.
         * @param pLogicalDevice      Renderer's logical device.
         * @param sShaderResourceName Name of the shader resource to look in the pipeline.
         * @param descriptorType      Type of descriptor to bind.
         * @param imageLayout         Image layout.
         * @param pSampler            Image sampler.
         * @param vImagesToBind       Images to bind (one per frame resource).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> rebindImageResourceToPipeline(
            VulkanPipeline* pVulkanPipeline,
            VkDevice pLogicalDevice,
            const std::string& sShaderResourceName,
            VkDescriptorType descriptorType,
            VkImageLayout imageLayout,
            VkSampler pSampler,
            std::array<VkImageView, FrameResourcesManager::getFrameResourcesCount()> vImagesToBind);

        /**
         * Initializes a new manager.
         *
         * @param pRenderer Used renderer.
         */
        LightingShaderResourceManager(Renderer* pRenderer);

        /**
         * Called by renderer when render resolution or projection matrix changes to queue a compute shader
         * that will recalculate grid of frustums used during light culling.
         *
         * @param renderTargetSize        New render of the underlying render image in pixels.
         * @param inverseProjectionMatrix Inverse projection matrix of the currently active camera.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> recalculateLightTileFrustums(
            const std::pair<unsigned int, unsigned int>& renderTargetSize,
            const glm::mat4& inverseProjectionMatrix);

        /**
         * Called by renderer to notify that all engine shaders were compiled.
         *
         * @remark Just changes internal boolean and nothing else.
         * Expects you to call @ref recalculateLightTileFrustums afterwards to do some actual calculations.
         */
        void onEngineShadersCompiled();

        /**
         * Sets light color intensity of ambient lighting.
         *
         * @remark New lighting settings will be copied to the GPU next time @ref updateResources is called.
         *
         * @param ambientLight Color in RGB format.
         */
        void setAmbientLight(const glm::vec3& ambientLight);

        /**
         * Updates all light source resources marked as "needs update" and copies new (updated) data to the
         * GPU resource of the specified frame resource.
         *
         * @warning Expected to be called somewhere inside of the `drawNextFrame` function
         * so that renderer's depth texture without multisampling pointer will not change.
         *
         * @remark Also copies data from @ref mtxGpuData.
         *
         * @param pCurrentFrameResource      Current frame resource that will be used to submit the next
         * frame.
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the
         * next frame.
         */
        void updateResources(FrameResource* pCurrentFrameResource, size_t iCurrentFrameResourceIndex);

        /**
         * Called after array of point light sources changed its size.
         *
         * @param iNewSize New size of the array that stores GPU data for spawned point lights.
         */
        void onPointLightArraySizeChanged(size_t iNewSize);

        /**
         * Called after array of indices to point lights in frustum was changed (indices changed).
         *
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the
         * next frame.
         */
        void onPointLightsInFrustumCulled(size_t iCurrentFrameResourceIndex);

        /**
         * Called after array of directional light sources changed its size.
         *
         * @param iNewSize New size of the array that stores GPU data for spawned directional lights.
         */
        void onDirectionalLightArraySizeChanged(size_t iNewSize);

        /**
         * Called after array of spotlights changed its size.
         *
         * @param iNewSize New size of the array that stores GPU data for spawned spotlights.
         */
        void onSpotlightArraySizeChanged(size_t iNewSize);

        /**
         * Called after array of indices to spotlights in frustum was changed (indices changed).
         *
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the
         * next frame.
         */
        void onSpotlightsInFrustumCulled(size_t iCurrentFrameResourceIndex);

        /**
         * Copies data from @ref mtxGpuData to the GPU resource of the current frame resource.
         *
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the
         * next frame.
         */
        void copyDataToGpu(size_t iCurrentFrameResourceIndex);

        /**
         * Updates descriptors in all graphics pipelines to make descriptors reference the underlying
         * buffers from @ref mtxGpuData.
         *
         * @remark Does nothing if DirectX renderer is used.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> rebindGpuDataToAllPipelines();

        /**
         * Updates descriptors in the specified graphics pipeline to make descriptors reference the underlying
         * buffers from @ref mtxGpuData.
         *
         * @remark Does nothing if DirectX renderer is used.
         *
         * @param pPipeline Pipeline to get descriptors from.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> rebindGpuDataToPipeline(Pipeline* pPipeline);

        /** Groups GPU resources that store arrays of light sources. */
        LightArrays lightArrays;

        /** Groups GPU related data. */
        std::pair<std::recursive_mutex, GpuData> mtxGpuData;

        /** Calculates frustum grid for light culling. */
        ComputeShaderData::FrustumGridComputeShader::ComputeShader frustumGridComputeShaderData;

        /** Does light culling. */
        ComputeShaderData::LightCullingComputeShader::ComputeShader lightCullingComputeShaderData;

        /** Compute interface that runs before light culling shader to reset global variables. */
        std::unique_ptr<ComputeShaderInterface> pPrepareLightCullingComputeInterface;

        /** Used renderer. */
        Renderer* pRenderer = nullptr;

        /** `true` if the renderer has finished compiling engine shaders, `false` otherwise. */
        bool bEngineShadersCompiled = false;

        /** Name of the resource that stores data from @ref mtxGpuData (name from shader code). */
        static inline const std::string sGeneralLightingDataShaderResourceName = "generalLightingData";

        /** Name of the resource that stores array of point lights. */
        static inline const std::string sPointLightsShaderResourceName = "pointLights";

        /** Name of the resource that stores array of directional lights. */
        static inline const std::string sDirectionalLightsShaderResourceName = "directionalLights";

        /** Name of the resource that stores array of spotlights. */
        static inline const std::string sSpotlightsShaderResourceName = "spotlights";

        /** Name of the resources that stores indices of point lights in camera's frustum. */
        static inline const std::string sPointLightsInCameraFrustumIndicesShaderResourceName =
            "pointLightsInCameraFrustumIndices";

        /** Name of the resources that stores indices of spotlights in camera's frustum. */
        static inline const std::string sSpotlightsInCameraFrustumIndicesShaderResourceName =
            "spotlightsInCameraFrustumIndices";

        /** Type of the descriptor used to store data from @ref mtxGpuData. */
        static constexpr auto generalLightingDataDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    };
}
