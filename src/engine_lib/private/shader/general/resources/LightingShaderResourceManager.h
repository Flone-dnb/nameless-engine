#pragma once

// Standard.
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>

// Custom.
#include "render/general/resources/UploadBuffer.h"
#include "render/general/resources/FrameResourcesManager.h"
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

            /** Total number of spawned point lights. */
            alignas(iVkScalarAlignment) unsigned int iPointLightCount = 0;

            /** Total number of spawned directional lights. */
            alignas(iVkScalarAlignment) unsigned int iDirectionalLightCount = 0;

            /** Total number of spawned spotlights. */
            alignas(iVkScalarAlignment) unsigned int iSpotlightCount = 0;
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

#if defined(WIN32)
        /**
         * Sets resource view of the specified lighting array to the specified command list.
         *
         * @warning Expects that the specified pipeline's shaders indeed use lighting resources.
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
                    pArray->mtxResources.second.vGpuResources[iCurrentFrameResourceIndex]
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
        ShaderLightArray* getPointLightDataArray();

        /**
         * Returns a non-owning reference to an array that stores data of all spawned directional lights.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Array that stores data of all spawned directional lights.
         */
        ShaderLightArray* getDirectionalLightDataArray();

        /**
         * Returns a non-owning reference to an array that stores data of all spawned spotlights.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Array that stores data of all spawned spotlights.
         */
        ShaderLightArray* getSpotlightDataArray();

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
                RootSignatureGenerator::getGeneralLightingConstantBufferRootParameterIndex(),
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
                pPointLightDataArray,
                sPointLightsShaderResourceName,
                RootSignatureGenerator::getPointLightsBufferRootParameterIndex());

            // Bind directional lights array.
            setLightingArrayViewToCommandList(
                pPso,
                pCommandList,
                iCurrentFrameResourceIndex,
                pDirectionalLightDataArray,
                sDirectionalLightsShaderResourceName,
                RootSignatureGenerator::getDirectionalLightsBufferRootParameterIndex());

            // Bind spotlights array.
            setLightingArrayViewToCommandList(
                pPso,
                pCommandList,
                iCurrentFrameResourceIndex,
                pSpotlightDataArray,
                sSpotlightsShaderResourceName,
                RootSignatureGenerator::getSpotlightsBufferRootParameterIndex());

#if defined(DEBUG)
            static_assert(
                sizeof(LightingShaderResourceManager) == 288, "consider adding new arrays here"); // NOLINT
#endif
        }
#endif

    private:
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

                    /** Maximum depth value. */
                    alignas(iVkScalarAlignment) float maxDepth = 0.0F;
                };

                /** Data that is used to convert coordinates from screen space to view space. */
                struct ScreenToViewData {
                    /** Inverse of the projection matrix. */
                    alignas(iVkMat4Alignment) glm::mat4 inverseProjectionMatrix = glm::identity<glm::mat4>();

                    /** Width of the viewport (might be smaller that the actual screen size). */
                    alignas(iVkScalarAlignment) unsigned int iRenderResolutionWidth = 0;

                    /** Height of the viewport (might be smaller that the actual screen size). */
                    alignas(iVkScalarAlignment) unsigned int iRenderResolutionHeight = 0;
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

                /** Groups shader interface and its resources. */
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
                     * @param renderResolution        Current render resolution.
                     * @param inverseProjectionMatrix Inverse projection matrix of the active camera.
                     * @param bQueueShaderExecution   `true` to queue compute shader execution, `false`
                     * otherwise.
                     *
                     * @return Error if something went wrong.
                     */
                    [[nodiscard]] std::optional<Error> updateData(
                        Renderer* pRenderer,
                        const std::pair<unsigned int, unsigned int>& renderResolution,
                        const glm::mat4& inverseProjectionMatrix,
                        bool bQueueShaderExecution);

                    /** Shader interface. */
                    std::unique_ptr<ComputeShaderInterface> pComputeInterface;

                    /** Shader resources. */
                    ShaderResources resources;

                    /**
                     * Total number of tiles in the X direction that was used when @ref updateData
                     * was called the last time.
                     */
                    unsigned int iLastUpdateTileCountX = 0;

                    /**
                     * Total number of tiles in the X direction that was used when @ref updateData
                     * was called the last time.
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

                    /**
                     * Stores global counters into various light index lists.
                     *
                     * @remark One resource per frame resource because before each frame we will reset
                     * global counters.
                     */
                    std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                        vGlobalCountersIntoLightIndexList;

                    /**
                     * Renderer's depth texture that we binded the last time.
                     *
                     * @remark Used to check if renderer's returned depth texture pointer is different
                     * from the one we binded the last time to rebind changed pointer to the shader,
                     * otherwise (if not changed) we will skip rebinding logic.
                     */
                    GpuResource* pLastBindedDepthTexture = nullptr;
                };

                /** Groups shader interface and its resources. */
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
                     * @param pDirectionalLightArray     Array that stores all spawned directional lights.
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
                        GpuResource* pDirectionalLightArray);

                    /** Shader interface. */
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
         * Initializes a new manager.
         *
         * @param pRenderer Used renderer.
         */
        LightingShaderResourceManager(Renderer* pRenderer);

        /**
         * Called by renderer when render resolution or projection matrix changes to queue a compute shader
         * that will recalculate grid of frustums used during light culling.
         *
         * @param renderResolution        New render resolution in pixels.
         * @param inverseProjectionMatrix Inverse projection matrix of the currently active camera.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> recalculateLightTileFrustums(
            const std::pair<unsigned int, unsigned int>& renderResolution,
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
         * Called after @ref pPointLightDataArray changed its size.
         *
         * @param iNewSize New size of the array that stores GPU data for spawned point lights.
         */
        void onPointLightArraySizeChanged(size_t iNewSize);

        /**
         * Called after @ref pDirectionalLightDataArray changed its size.
         *
         * @param iNewSize New size of the array that stores GPU data for spawned directional lights.
         */
        void onDirectionalLightArraySizeChanged(size_t iNewSize);

        /**
         * Called after @ref pSpotlightDataArray changed its size.
         *
         * @param iNewSize New size of the array that stores GPU data for spawned spotlights.
         */
        void onSpotlightArraySizeChanged(size_t iNewSize);

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

        /** Stores data of all spawned point lights. */
        std::unique_ptr<ShaderLightArray> pPointLightDataArray;

        /** Stores data of all spawned directional lights. */
        std::unique_ptr<ShaderLightArray> pDirectionalLightDataArray;

        /** Stores data of all spawned spotlights. */
        std::unique_ptr<ShaderLightArray> pSpotlightDataArray;

        /** Groups GPU related data. */
        std::pair<std::recursive_mutex, GpuData> mtxGpuData;

        /** Calculates frustum grid for light culling. */
        ComputeShaderData::FrustumGridComputeShader::ComputeShader frustumGridComputeShaderData;

        /** Does light culling. */
        ComputeShaderData::LightCullingComputeShader::ComputeShader lightCullingComputeShaderData;

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

        /** Type of the descriptor used to store data from @ref mtxGpuData. */
        static constexpr auto generalLightingDataDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    };
}
