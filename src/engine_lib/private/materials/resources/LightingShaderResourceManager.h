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
#include "materials/hlsl/RootSignatureGenerator.h"
#include "render/directx/pipeline/DirectXPso.h"
#endif

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

    class ShaderLightArray;
    class Renderer;
    class Pipeline;

    /**
     * RAII-like object that frees the slot (marks it as unused) in its destructor and stores all
     * information needed to update the slot's data.
     */
    class ShaderLightArraySlot {
        // Only arrays can create slots.
        friend class ShaderLightArray;

    public:
        ShaderLightArraySlot() = delete;

        ShaderLightArraySlot(const ShaderLightArraySlot&) = delete;
        ShaderLightArraySlot& operator=(const ShaderLightArraySlot&) = delete;

        ~ShaderLightArraySlot();

        /**
         * Marks resources as "needs update", the resource will not be updated immediately but only
         * when it will be safe to modify the underlying GPU resource of the array that this slot is
         * referencing.
         *
         * @remark Causes update callbacks to be called multiple times later.
         */
        void markAsNeedsUpdate();

    private:
        /**
         * Creates a new slot.
         *
         * @param pArray                Array that allocated space for this slot.
         * @param iIndexIntoArray       Index into array.
         * @param startUpdateCallback   Callback that will be called by array to start copying new data
         * to the GPU.
         * @param finishUpdateCallback  Callback that will be called by array to finish copying new data
         * to the GPU.
         */
        ShaderLightArraySlot(
            ShaderLightArray* pArray,
            size_t iIndexIntoArray,
            const std::function<void*()>& startUpdateCallback,
            const std::function<void()>& finishUpdateCallback);

        /** Array that allocated space for this slot. */
        ShaderLightArray* pArray = nullptr;

        /** Callback that will be called by array to start copying new data to the GPU. */
        const std::function<void*()> startUpdateCallback;

        /** Callback that will be called by array to finish copying new data to the GPU. */
        const std::function<void()> finishUpdateCallback;

        /** Index into @ref pArray. */
        size_t iIndexIntoArray = 0;
    };

    /**
     * Manages array(s) (defined in shaders) related to lighting and allows modifying array data
     * from CPU side.
     */
    class ShaderLightArray {
        // Frees the slot in its destructor.
        friend class ShaderLightArraySlot;

        // Manager notifies when pipeline updates happen and when we can copy new data of slots that
        // need update.
        friend class LightingShaderResourceManager;

    public:
        /** Groups used resources. */
        struct Resources {
            /**
             * GPU resource per frame "in-flight" that stores array of light data.
             *
             * @remark Resources in this array have equal sizes.
             *
             * @remark Resources in this array are always valid and always have space for at least one slot
             * (even if there are no slots active) to avoid hitting `nullptr` or have branching
             * when binding resources (when there are no active slots these resources will not be
             * used since counter for light sources will be zero but we will have a valid binding).
             *
             * @remark Storing a resource per frame "in-flight" because we should not update a resource
             * that is currently being used by the GPU but we also don't want to stop the rendering when
             * we need an update.
             */
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vGpuResources;

            /** Slots (elements) in arrays from @ref vGpuResources that needs to be updated. */
            std::array<
                std::unordered_set<ShaderLightArraySlot*>,
                FrameResourcesManager::getFrameResourcesCount()>
                vSlotsToBeUpdated;

            /** All currently active (existing) slots. */
            std::unordered_set<ShaderLightArraySlot*> activeSlots;
        };

        ShaderLightArray() = delete;

        /** Makes sure there are no active slots. */
        ~ShaderLightArray();

        /**
         * Creates a new array.
         *
         * @param pRenderer                Used renderer.
         * @param sShaderLightResourceName Name of the resource (specified in shader code) that this array
         * should bind to.
         * @param onSizeChanged            Callback that will be called after array's size changed
         * with the current array size passed as the only argument.
         *
         * @return Created array.
         */
        static std::unique_ptr<ShaderLightArray> create(
            Renderer* pRenderer,
            const std::string& sShaderLightResourceName,
            const std::function<void(size_t)>& onSizeChanged);

        /**
         * Reserves a new slot in the array to store some data.
         *
         * @remark While you hold the returned slot-object (and while it's not destroyed),
         * it can call update callbacks at any time.
         *
         * @remark Update callbacks will be called inside of this function to copy the initial data.
         *
         * @remark If you mark your slot as "needs update" callbacks may be called multiple times
         * (this is perfectly fine, just don't rely on your callbacks being called only once).
         *
         * @param iDataSizeInBytes     Size of the data that you want to store in the slot in bytes.
         * @param startUpdateCallback  Callback that will be called after you mark your slot as "needs update"
         * when the engine is ready to copy the data to the GPU. You must return a pointer which data will
         * be copied.
         * @param finishUpdateCallback Callback that will be called after the copying of your new data is
         * finished.
         *
         * @return Error if something went wrong, otherwise reserved slot.
         */
        std::variant<std::unique_ptr<ShaderLightArraySlot>, Error> reserveNewSlot(
            size_t iDataSizeInBytes,
            const std::function<void*()>& startUpdateCallback,
            const std::function<void()>& finishUpdateCallback);

        /**
         * Returns internal resources of this array.
         *
         * @remark Generally used for tests (read-only), you should not modify them.
         *
         * @return Internal resources.
         */
        std::pair<std::recursive_mutex, Resources>* getInternalResources();

    private:
        /**
         * Creates a new array.
         *
         * @warning Only used internally, prefer to use @ref create.
         *
         * @param pRenderer                 Used renderer.
         * @param sShaderLightResourceName  Name of the resource (specified in shader code) that this array
         * should bind to.
         * @param onSizeChanged             Callback that will be called after array's size changed
         * with the current array size passed as the only argument.
         */
        ShaderLightArray(
            Renderer* pRenderer,
            const std::string& sShaderLightResourceName,
            const std::function<void(size_t)>& onSizeChanged);

        /**
         * (Re)creates GPU resources to hold the current number of active slots and
         * updates all previously existing slots.
         *
         * @param bIsInitialization `true` if we are creating resources for the first time and there
         * are no active slots at the moment but since we need valid (non `nullptr` resources) we will
         * create resources that will hold 1 slot, `false` otherwise.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> recreateArray(bool bIsInitialization = false);

        /**
         * Goes through all slots that are marked as "needs update" and copies their new data to the GPU
         * resource.
         *
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the next
         * frame.
         */
        void updateSlotsMarkedAsNeedsUpdate(size_t iCurrentFrameResourceIndex);

        /**
         * Binds the underlying GPU resource to descriptors of pipelines that use this array in shaders.
         *
         * @remark Does nothing if DirectX renderer is used.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateBindingsInAllPipelines();

        /**
         * Binds the underlying GPU resource to the specified pipeline's descriptor (if this pipeline's
         * shaders use this array, otherwise return empty).
         *
         * @remark Does nothing if DirectX renderer is used.
         *
         * @param pPipeline Pipeline to bind to / rebind to.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updatePipelineBinding(Pipeline* pPipeline);

        /**
         * Called by slot-objects to notify the array that a slot is no longer used.
         *
         * @param pSlot Slot that is being destroyed.
         */
        void freeSlot(ShaderLightArraySlot* pSlot);

        /**
         * Queues the specified slot's data to be updated later.
         *
         * @param pSlot Slot to update later.
         */
        void markSlotAsNeedsUpdate(ShaderLightArraySlot* pSlot);

        /** Internal data. */
        std::pair<std::recursive_mutex, Resources> mtxResources;

        /** Used renderer. */
        Renderer* pRenderer = nullptr;

        /** Size of one array element in bytes. */
        size_t iElementSizeInBytes = 0;

        /**
         * Callback that will be called after array's size changed with the current array size passed as the
         * only argument.
         */
        const std::function<void(size_t)> onSizeChanged;

        /** Name of the resource (specified in shader code) that this array should bind to. */
        const std::string sShaderLightResourceName;
    };

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
                sizeof(LightingShaderResourceManager) == 176, "consider adding new arrays here"); // NOLINT
#endif
        }
#endif

    private:
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
         * @remark Also copies data from @ref mtxGpuData.
         *
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the
         * next frame.
         */
        void updateResources(size_t iCurrentFrameResourceIndex);

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

        /** Used renderer. */
        Renderer* pRenderer = nullptr;

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
