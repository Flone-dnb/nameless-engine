#pragma once

// Standard.
#include <variant>
#include <memory>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <queue>
#include <functional>

// Custom.
#include "render/directx/descriptors/DirectXDescriptor.h"
#include "render/general/resources/GpuResource.h"
#include "DirectXDescriptorType.hpp"

// External.
#include "directx/d3dx12.h"
#include "misc/Error.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;
    class DirectXResource;
    class DirectXDescriptorHeap;

    /** Defines types of different descriptor heaps. */
    enum class DescriptorHeapType : int {
        RTV = 0,
        DSV,
        CBV_SRV_UAV,
    };

    /**
     * Works as a mini descriptor heap that operates on descriptors in a continuous range
     * (can be used for bindless bindings of descriptor arrays).
     *
     * @remark Size of this range automatically changes (expands/shrinks) depending on the usage.
     */
    class ContinuousDirectXDescriptorRange {
        // This class is fully managed by the heap.
        friend class DirectXDescriptorHeap;

    public:
        ContinuousDirectXDescriptorRange() = delete;

        /* Notifies the heap. */
        ~ContinuousDirectXDescriptorRange();

        ContinuousDirectXDescriptorRange(const ContinuousDirectXDescriptorRange& other) = delete;
        ContinuousDirectXDescriptorRange& operator=(const ContinuousDirectXDescriptorRange& other) = delete;

        // Intentionally disable `move` because heap will store a raw pointer to the range
        // and we don't want to accidentally `move` the range which will make the heap's raw pointer invalid.
        ContinuousDirectXDescriptorRange(ContinuousDirectXDescriptorRange&& other) noexcept = delete;
        ContinuousDirectXDescriptorRange&
        operator=(ContinuousDirectXDescriptorRange&& other) noexcept = delete;

        /**
         * Returns grow/shrink size for all continuous descriptor ranges.
         *
         * @return Grow/shrink size.
         */
        static constexpr INT getRangeGrowSize() { return iRangeGrowSize; }

        /**
         * Returns the number of active (currently in-use) descriptors that were allocated from this range.
         *
         * @return Active descriptors.
         */
        size_t getRangeSize();

        /**
         * Returns the total number of descriptors that this range can currently store.
         *
         * @return Total capacity.
         */
        size_t getRangeCapacity();

        /**
         * Returns index of the first descriptor of this range in the heap.
         *
         * @return Negative if not initialized yet, otherwise descriptor index in the heap.
         */
        INT getRangeStartInHeap();

        /**
         * Returns a GPU descriptor handle to the current range start.
         *
         * @warning Returned handle is only valid for limited amount of time, until range is moved to other
         * place in heap (because heap is being re-created for example).
         *
         * @return GPU descriptor handle to range start.
         */
        D3D12_GPU_DESCRIPTOR_HANDLE getGpuDescriptorHandleToRangeStart() const;

    private:
        /** Groups mutex guarded data. */
        struct InternalData {
            /**
             * Descriptors allocated from this range.
             *
             * @remark Size of this set defines range size (actually used descriptor count).
             *
             * @remark It's safe to store raw pointers here because descriptor in its destructor will
             * notify the range.
             */
            std::unordered_set<DirectXDescriptor*> allocatedDescriptors;

            /** Indices (relative to heap start) of descriptors that were created but no longer being used. */
            std::queue<INT> noLongerUsedDescriptorIndices;

            /**
             * Index of the first descriptor of this range in the heap.
             *
             * @remark `-1` means that no space was reserved (i.e. not initialized), this is used to determine
             * if we should call the notification callback or not.
             */
            INT iRangeStartInHeap = -1;

            /** Current range capacity. */
            INT iRangeCapacity = 0;

            /**
             * Index of the next free descriptor (relative to the range start @ref iRangeStartInHeap) that can
             * be used.
             *
             * @remark Once this value is equal to @ref iRangeCapacity we will use @ref
             * noLongerUsedDescriptorIndices to see if any old descriptors were released and no longer being
             * used.
             */
            INT iNextFreeIndexInRange = 0;
        };

        /**
         * Creates a new range (with capacity @ref iRangeGrowSize but no descriptor is used) allocated in a
         * descriptor heap.
         *
         * @param pHeap                 Heap that allocated the range.
         * @param onRangeIndicesChanged Callback that will be called after the range was moved in the heap due
         * to things like heap expand/shrink.
         * @param sRangeName            Name of this range (used for logging).
         */
        ContinuousDirectXDescriptorRange(
            DirectXDescriptorHeap* pHeap,
            const std::function<void()>& onRangeIndicesChanged,
            const std::string& sRangeName);

        /**
         * Removes the specified descriptor from the range and marks descriptor's index
         * as unused.
         *
         * @remark Does not shrink the range.
         *
         * @param pDescriptor Descriptor to remove.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> markDescriptorAsUnused(DirectXDescriptor* pDescriptor);

        /**
         * Looks if there is a free index in the range that can be used to create a new descriptor
         * or if there is no space (expansion needed).
         *
         * @remark Marks returned index as "in-use" and expects a new descriptor to be assigned to the range
         * after this function.
         *
         * @return Error if something went wrong, otherwise empty if no free space (expansion needed)
         * or an index relative to heap start.
         */
        std::variant<std::optional<INT>, Error> tryReserveFreeHeapIndexToCreateDescriptor();

        /** Internal data. */
        std::pair<std::recursive_mutex, InternalData> mtxInternalData;

        /** Called after the range was moved in the heap due to things like heap expand/shrink. */
        const std::function<void()> onRangeIndicesChanged;

        /** Name of this range (used for logging). */
        const std::string sRangeName;

        /** Heap that allocated the range. */
        DirectXDescriptorHeap* const pHeap = nullptr;

        /**
         * Grow/shrink size for all ranges. Constant for all ranges because it causes the heap to be
         * re-created and we want to avoid small ranges and big ranges (due to how heap expands/shrinks)
         * which also depends on the heap grow size.
         */
        static constexpr INT iRangeGrowSize = 50; // NOLINT: see static asserts
    };

    /** Represents a descriptor heap. */
    class DirectXDescriptorHeap {
        // Notifies the heap about descriptor being destroyed.
        friend class DirectXDescriptor;

        // Notifies the heap about range being destroyed.
        friend class ContinuousDirectXDescriptorRange;

        // Only resource can request descriptors.
        friend class DirectXResource;

    public:
        /** Groups mutex guarded data. */
        struct InternalData {
            /** Descriptor heap. */
            ComPtr<ID3D12DescriptorHeap> pHeap;

            /** Descriptor ranges that were allocated in this heap. */
            std::unordered_set<ContinuousDirectXDescriptorRange*> continuousDescriptorRanges;

            /** Current heap capacity. */
            INT iHeapCapacity = 0;

            /**
             * Current heap size (actually used size).
             *
             * @remark Includes capacity of ranges from @ref continuousDescriptorRanges.
             */
            INT iHeapSize = 0;

            /**
             * Index of the next free descriptor that can be used. Each created descriptor will fetch this
             * value (to be used) and increment it.
             *
             * @remark Once this value is equal to @ref iHeapCapacity we will use @ref
             * noLongerUsedSingleDescriptorIndices to see if any old descriptors were released and no longer
             * being used.
             */
            INT iNextFreeHeapIndex = 0;

            /**
             * Indices of descriptors that were created but no longer being used.
             *
             * @remark Does not include unused descriptor indices of ranges from @ref
             * continuousDescriptorRanges.
             */
            std::queue<INT> noLongerUsedSingleDescriptorIndices;

            /**
             * Set of descriptors that use this heap (size of this set might not be equal to @ref iHeapSize
             * due to @ref continuousDescriptorRanges, because this set stores single descriptors
             * (that don't belong to a continuous range)).
             *
             * @remark Storing a raw pointer here because it's only used to update view if the heap was
             * recreated (no resource ownership). Once resource is destroyed the descriptor will also be
             * destroyed and thus it will be removed from this set.
             */
            std::unordered_set<DirectXDescriptor*> bindedSingleDescriptors;
        };

        DirectXDescriptorHeap() = delete;
        DirectXDescriptorHeap(const DirectXDescriptorHeap&) = delete;
        DirectXDescriptorHeap& operator=(const DirectXDescriptorHeap&) = delete;

        /** Makes sure no descriptor or range exists. */
        ~DirectXDescriptorHeap();

        /**
         * Returns grow/shrink size for the heap.
         *
         * @return Grow/shrink size.
         */
        static constexpr INT getHeapGrowSize() { return iHeapGrowSize; }

        /**
         * Creates a new manager that controls a specific heap.
         *
         * @param pRenderer DirectX renderer that owns this manager.
         * @param heapType  Heap type that this manager will control.
         *
         * @return Error if something went wrong, otherwise pointer to the manager.
         */
        static std::variant<std::unique_ptr<DirectXDescriptorHeap>, Error>
        create(DirectXRenderer* pRenderer, DescriptorHeapType heapType);

        /**
         * Allocates a continuous range of descriptors that can be used for bindless bindings of descriptor
         * arrays.
         *
         * @param sRangeName            Name of the range (used for logging).
         * @param onRangeIndicesChanged Callback that will be called after the range was moved in the heap due
         * to things like heap expand/shrink.
         *
         * @return Error if something went wrong, otherwise allocated descriptor range.
         */
        std::variant<std::unique_ptr<ContinuousDirectXDescriptorRange>, Error>
        allocateContinuousDescriptorRange(
            const std::string& sRangeName, const std::function<void()>& onRangeIndicesChanged);

        /**
         * Returns current heap capacity (allocated heap size).
         *
         * This function is used for engine testing and generally should not be used
         * outside of testing.
         *
         * @return Heap capacity.
         */
        INT getHeapCapacity();

        /**
         * Returns current heap size (actually used heap size).
         *
         * This function is used for engine testing and generally should not be used
         * outside of testing.
         *
         * @return Heap size.
         */
        INT getHeapSize();

        /**
         * Returns amount of descriptors that were created but no longer being used.
         *
         * @return Descriptor count.
         */
        size_t getNoLongerUsedDescriptorCount();

        /**
         * Returns size of one descriptor in this heap.
         *
         * @return Descriptor size.
         */
        inline UINT getDescriptorSize() const { return iDescriptorSize; }

        /**
         * Returns internal DirectX heap.
         *
         * @return DirectX heap.
         */
        inline ID3D12DescriptorHeap* getInternalHeap() const { return mtxInternalData.second.pHeap.Get(); }

        /**
         * Returns internal data of the object.
         *
         * @warning Only used for read-only purposes of automated-tests.
         *
         * @return Internal data.
         */
        std::pair<std::recursive_mutex, InternalData>* getInternalData() { return &mtxInternalData; }

    protected:
        /**
         * Converts heap type to string.
         *
         * @param heapType Heap type to convert to text.
         *
         * @return Heap type text.
         */
        static std::string convertHeapTypeToString(DescriptorHeapType heapType);

        /**
         * Constructor.
         *
         * @param pRenderer DirectX renderer that owns this manager.
         * @param heapType  Type of the heap.
         */
        DirectXDescriptorHeap(DirectXRenderer* pRenderer, DescriptorHeapType heapType);

        /**
         * Marks resource descriptor as no longer being used so the descriptor's index can be reused by some
         * other descriptor.
         *
         * @remark Called from DirectXDescriptor destructor.
         *
         * @param pDescriptor Descriptor that is being destroyed.
         * @param pRange      Range that allocated the descriptor (if the descriptor was allocated from a
         * range).
         */
        void onDescriptorBeingDestroyed(
            DirectXDescriptor* pDescriptor, ContinuousDirectXDescriptorRange* pRange = nullptr);

        /**
         * Notifies the heap about some range being destroyed.
         *
         * @remark Called from range destructor.
         *
         * @param pRange Range that is being destroyed.
         */
        void onDescriptorRangeBeingDestroyed(ContinuousDirectXDescriptorRange* pRange);

        /**
         * Creates a new view using the specified descriptor handle that will point to
         * the specified resource.
         *
         * @param heapHandle        A place in the heap to create view.
         * @param pResource         Resource to bind to the new view.
         * @param descriptorType    Descriptor type.
         * @param cubemapFaceIndex  Specify empty if creating a view for the entire resource, otherwise
         * specify index of the cubemap face to bind the descriptor to.
         */
        void createView(
            CD3DX12_CPU_DESCRIPTOR_HANDLE heapHandle,
            const DirectXResource* pResource,
            DirectXDescriptorType descriptorType,
            std::optional<size_t> cubemapFaceIndex) const;

        /**
         * Re-creates the heap to expand its capacity to support @ref iHeapGrowSize more descriptors.
         *
         * @remark All internal values and old descriptors will be updated.
         *
         * @param pChangedRange If this event was caused by changes in descriptor range, specify
         * one for logging purposes.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> expandHeap(ContinuousDirectXDescriptorRange* pChangedRange);

        /**
         * Checks if the heap can be shrinked (based on the current internal state @ref mtxInternalData) and
         * if possible re-creates the heap to shrink its capacity to support @ref iHeapGrowSize less
         * descriptors.
         *
         * @remark All internal values and old descriptors will be updated.
         *
         * @param pChangedRange If this event was caused by changes in descriptor range, specify
         * one for logging purposes.
         *
         * @return Error if something went wrong, otherwise `true` if heap was shrinked, `false` if no
         * shrinking is needed yet.
         */
        [[nodiscard]] std::variant<bool, Error>
        shrinkHeapIfPossible(ContinuousDirectXDescriptorRange* pChangedRange);

        /**
         * (Re)creates the heap with the specified capacity.
         *
         * @remark Only updates the heap resource, internal capacity and updates old descriptors
         * (if any), other internal values are not changed and should be corrected afterwards.
         *
         * @param iCapacity     Size of the heap (in descriptors).
         * @param pChangedRange If this event was caused by changes in descriptor range, specify
         * one for logging purposes.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        createHeap(INT iCapacity, ContinuousDirectXDescriptorRange* pChangedRange);

        /**
         * Returns an array of descriptor types that this heap handles.
         * For example: for RTV heap it will only be RTV descriptor type,
         * for DSV - DSV, for CBV/UAV/SRV heap it will be CBV, UAV and SRV (3 types).
         *
         * @return Descriptor types that this heap handles.
         */
        std::vector<DirectXDescriptorType> getDescriptorTypesHandledByThisHeap() const;

        /**
         * Recreates views for previously created descriptors so that they will reference
         * the new re-created heap and reference the correct index inside of the heap.
         *
         * @remark Also updates next free descriptor index and queue of unused indices.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> rebindViewsUpdateIndices();

    private:
        /**
         * Creates a new descriptor that points to the given resource,
         * the descriptor is saved in the resource.
         *
         * @remark You can use this function to assign a different descriptor to already created resource.
         * For example: create SRV resource using resource manager and use RTV heap to assign
         * a RTV descriptor to this resource so it will have 2 different descriptors.
         *
         * @param pResource      Resource to point new descriptor to.
         * @param descriptorType Type of the new descriptor.
         * @param pRange         Specify in order to allocate a descriptor from this range.
         * @param bBindDescriptorsToCubemapFaces If this resource is a cubemap, specify `true`
         * to also bind descriptors that reference specific cubemap faces, specify `false` to only bind 1
         * descriptor that references the entire resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> assignDescriptor(
            DirectXResource* pResource,
            DirectXDescriptorType descriptorType,
            ContinuousDirectXDescriptorRange* pRange = nullptr,
            bool bBindDescriptorsToCubemapFaces = true);

        /**
         * Checks shrink condition: if capacity can be decremented by grow size with the current size.
         *
         * @param iSize     Current size.
         * @param iCapacity Current capacity.
         * @param iGrowSize Grow/shrink size. Expected to be even.
         *
         * @return `true` if capacity can be decremented by one grow size, `false` if not yet.
         */
        static bool isShrinkingPossible(INT iSize, INT iCapacity, INT iGrowSize);

        /**
         * Expands the specified descriptor range and expands or re-creates the heap to support updated range.
         *
         * @param pRange Range to expand.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> expandRange(ContinuousDirectXDescriptorRange* pRange);

        /** Do not delete. Owner renderer. */
        DirectXRenderer* pRenderer;

        /** Descriptor heap internal resources. */
        std::pair<std::recursive_mutex, InternalData> mtxInternalData;

        /** Size of one descriptor. */
        UINT iDescriptorSize = 0;

        /** Type of the heap. */
        DescriptorHeapType heapType;

        /** String version of heap type (used for logging). */
        std::string sHeapType;

        /** Direct3D type of this heap. */
        D3D12_DESCRIPTOR_HEAP_TYPE d3dHeapType;

        /** Number of descriptors to add to the heap when there is no more free space left. */
        static constexpr INT iHeapGrowSize = 300; // NOLINT: don't recreate heap too often
    };
} // namespace ne
