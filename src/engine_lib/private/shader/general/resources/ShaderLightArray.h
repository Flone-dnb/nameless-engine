#pragma once

// Standard.
#include <memory>
#include <functional>
#include <variant>
#include <optional>
#include <array>
#include <unordered_set>
#include <mutex>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/UploadBuffer.h"
#include "render/general/resources/frame/FrameResourcesManager.h"

namespace ne {
    class Renderer;
    class Pipeline;
    class ShaderLightArray;
    class Node;

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

        /**
         * Returns the current index (because it may change later) into the array.
         *
         * @return Index.
         */
        inline size_t getCurrentIndexIntoArray() const { return iIndexIntoArray; }

    private:
        /**
         * Creates a new slot.
         *
         * @param pArray                 Array that allocated space for this slot.
         * @param pSpawnedOwnerLightNode Spawned light node that requested the slot.
         * @param iIndexIntoArray        Index into array.
         * @param startUpdateCallback    Callback that will be called by array to start copying new data
         * to the GPU.
         * @param finishUpdateCallback   Callback that will be called by array to finish copying new data
         * to the GPU.
         */
        ShaderLightArraySlot(
            ShaderLightArray* pArray,
            Node* pSpawnedOwnerLightNode,
            size_t iIndexIntoArray,
            const std::function<void*()>& startUpdateCallback,
            const std::function<void()>& finishUpdateCallback);

        /** Array that allocated space for this slot. */
        ShaderLightArray* pArray = nullptr;

        /**
         * Spawned light node (point/spot/directional/etc) that requested this slot.
         *
         * @remark Do not delete (free) this pointer. It's safe to store a raw pointer here because when
         * a light node despawns it destroys this slot object so this pointer is always valid.
         */
        Node* const pSpawnedOwnerLightNode = nullptr;

        /** Callback that will be called by array to start copying new data to the GPU. */
        const std::function<void*()> startUpdateCallback;

        /** Callback that will be called by array to finish copying new data to the GPU. */
        const std::function<void()> finishUpdateCallback;

        /** Index into @ref pArray. */
        size_t iIndexIntoArray = 0;
    };

    /**
     * Manages an array (defined in shaders) related to lighting and allows modifying array data
     * from CPU side.
     */
    class ShaderLightArray {
        // Frees the slot in its destructor.
        friend class ShaderLightArraySlot;

        // Manager notifies when pipeline updates happen and when we can copy new data of slots that
        // need update.
        friend class LightingShaderResourceManager;

        // Modifies lights in frustum.
        friend class Renderer;

    public:
        /** Groups resources related to light sources in active camera's frustum. */
        struct LightsInFrustum {
            /**
             * Array of light nodes of the same type (if this shader light array object is used only for
             * directional lights then only directional light nodes are stored in this array) where positions
             * of light nodes in this array correspond to positions of their light data from
             * @ref vGpuResources. So light data at index 0 in the array used in shaders is taken from
             * the light node stored in this array at index 0.
             *
             * @remark For example, you can use indices of light nodes in this array to tell the shaders
             * which lights to process in shaders and which to ignore.
             */
            std::vector<Node*> vShaderLightNodeArray;

            /**
             * Stores indices to elements from @ref vLightIndicesInFrustum that are located inside of
             * active camera's frustum.
             */
            std::vector<unsigned int> vLightIndicesInFrustum;

            /**
             * GPU resources that store @ref vLightIndicesInFrustum.
             *
             * @remark Resources in this array are always valid if you specified that you need an index array
             * for this light array (when creating shader light array object) and always have space for at
             * least one item (even if there are no lights spawned) to avoid hitting `nullptr` or have
             * branching when binding resources (when there are no active lights these resources will not be
             * used since counter for light sources will be zero but we will have a valid binding).
             */
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vGpuResources;

            /**
             * Name of the shader resource that stores indices to lights in camera's frustum.
             *
             * @remark Empty if array of indices is not used.
             */
            std::string sShaderResourceName;
        };

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
                vGpuArrayLightDataResources;

            /** Stores information about light sources in active camera's frustum. */
            LightsInFrustum lightsInFrustum;

            /** Slots (elements) in arrays from @ref vGpuArrayLightDataResources that needs to be updated. */
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
         * @param pRenderer                      Used renderer.
         * @param sShaderLightResourceName       Name of the resource (specified in shader code) that this
         * array should bind to.
         * @param onSizeChanged                  Callback that will be called after array's size changed
         * with the current array size passed as the only argument.
         * @param optionalOnLightsInCameraFrustumCulled A pair of callback and shader resource name that are
         * used for array that stores indices of light sources in camera's frustum. If specified the callback
         * will be called after array of indices to lights in camera frustum changed (indices changed) with
         * the current frame resource index as the only argument, otherwise (if empty) GPU resources for such
         * array will not be created and this callback will never be called.
         *
         * @return Created array.
         */
        static std::unique_ptr<ShaderLightArray> create(
            Renderer* pRenderer,
            const std::string& sShaderLightResourceName,
            const std::function<void(size_t)>& onSizeChanged,
            const std::optional<std::pair<std::function<void(size_t)>, std::string>>&
                optionalOnLightsInCameraFrustumCulled);

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
         * @param pSpawnedOwnerLightNode Spawned light node that requests the slot. Used for light culling.
         * @param iDataSizeInBytes       Size of the data that you want to store in the slot in bytes.
         * @param startUpdateCallback    Callback that will be called after you mark your slot as "needs
         * update" when the engine is ready to copy the data to the GPU. You must return a pointer which data
         * will be copied.
         * @param finishUpdateCallback   Callback that will be called after the copying of your new data is
         * finished.
         *
         * @return Error if something went wrong, otherwise reserved slot.
         */
        std::variant<std::unique_ptr<ShaderLightArraySlot>, Error> reserveNewSlot(
            Node* pSpawnedOwnerLightNode,
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

        /**
         * Returns name of the resource (specified in shader code) that this array is binded to.
         *
         * @return Shader resource name.
         */
        std::string getShaderResourceName() const;

    private:
        /**
         * Creates a new array.
         *
         * @warning Only used internally, prefer to use @ref create.
         *
         * @param pRenderer                      Used renderer.
         * @param sShaderLightResourceName       Name of the resource (specified in shader code) that this
         * array should bind to.
         * @param onSizeChanged                  Callback that will be called after array's size changed
         * with the current array size passed as the only argument.
         * @param optionalCallbackOnLightsInCameraFrustumCulled If specified will be called after array of
         * indices to lights in camera frustum changed (indices changed) with the current frame resource index
         * as the only argument, otherwise (if empty) GPU resources for such array will not be created and
         * this callback will never be called.
         * @param sIndicesLightsInFrustumShaderResourceName If callback for culled lights in camera frustum
         * is specified stores name of the shader resource used for the array of indices of non-culled lights.
         */
        ShaderLightArray(
            Renderer* pRenderer,
            const std::string& sShaderLightResourceName,
            const std::function<void(size_t)>& onSizeChanged,
            const std::optional<std::function<void(size_t)>>& optionalCallbackOnLightsInCameraFrustumCulled,
            const std::string& sIndicesLightsInFrustumShaderResourceName = "");

        /**
         * Called after the renderer culls lights (so that indices of lights sources in camera's frustum
         * change) to copy the new (modified) data to the GPU.
         *
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the next
         * frame.
         */
        void onLightsInCameraFrustumCulled(size_t iCurrentFrameResourceIndex);

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
         * If specified will be called after array of indices to lights in camera frustum changed
         * (indices changed) with the current frame resource index as the only argument, otherwise (if empty)
         * GPU resources for such array will not be created and this callback will never be called.
         */
        const std::optional<std::function<void(size_t)>> optionalCallbackOnLightsInCameraFrustumCulled;

        /**
         * Callback that will be called after array's size changed with the current array size passed as the
         * only argument.
         */
        const std::function<void(size_t)> onSizeChanged;

        /** Name of the resource (specified in shader code) that this array should bind to. */
        const std::string sShaderLightResourceName;
    };
}
