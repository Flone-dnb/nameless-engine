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
#include "render/general/resources/FrameResourcesManager.h"

namespace ne {
    class Renderer;
    class Pipeline;
    class ShaderLightArray;

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
     * Manages an array (defined in shaders) related to lighting and allows modifying array data
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
}