#pragma once

// Standard.
#include <variant>
#include <memory>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <queue>

// Custom.
#include "render/directx/descriptors/DirectXDescriptor.h"

// External.
#include "directx/d3dx12.h"
#include "misc/Error.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;
    class DirectXResource;

    /** Defines types of different descriptor heaps. */
    enum class DescriptorHeapType : int {
        RTV = 0,
        DSV,
        CBV_SRV_UAV,
    };

    /** Represents a descriptor heap. */
    class DirectXDescriptorHeap {
    public:
        DirectXDescriptorHeap() = delete;
        DirectXDescriptorHeap(const DirectXDescriptorHeap&) = delete;
        DirectXDescriptorHeap& operator=(const DirectXDescriptorHeap&) = delete;

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
         * Creates a new descriptor that points to the given resource,
         * the descriptor is saved in the resource.
         *
         * @param pResource      Resource to point new descriptor to.
         * @param descriptorType Type of the new descriptor.
         *
         * @remark Prefer to use DirectXResource's "addRtv", "addDsv" and other functions instead of this one.
         *
         * @remark You can use this function to assign a different descriptor to already created resource.
         * For example: create SRV resource using resource manager and use RTV heap to assign
         * a RTV descriptor to this resource so it will have 2 different descriptors.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> assignDescriptor(DirectXResource* pResource, DescriptorType descriptorType);

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

    protected:
        friend class DirectXDescriptor;

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
         * Marks resource descriptor(s) as no longer being used
         * so they can be reused by some other resource.
         *
         * Called from DirectXDescriptor destructor.
         *
         * @param pResource Resource that is using descriptors.
         */
        void markDescriptorAsNoLongerBeingUsed(DirectXResource* pResource);

        /**
         * Creates a new view using the specified descriptor handle that will point to
         * the specified resource.
         *
         * @param heapHandle A place in the heap to create view.
         * @param pResource  Resource to bind to the new view.
         * @param descriptorType Descriptor type.
         */
        void createView(
            CD3DX12_CPU_DESCRIPTOR_HANDLE heapHandle,
            const DirectXResource* pResource,
            DescriptorType descriptorType) const;

        /**
         * Recreates the heap to expand it to another @ref iHeapGrowSize descriptors.
         * Old descriptors will be updates.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> expandHeap();

        /**
         * Recreates the heap to expand it to another @ref iHeapGrowSize descriptors.
         * Old descriptors will be updates.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> shrinkHeap();

        /**
         * (Re)creates the heap with the specified capacity.
         * Old descriptors (if any) will be updates.
         *
         * @param iCapacity Size of the heap (in descriptors).
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> createHeap(INT iCapacity);

        /**
         * Returns an array of descriptor types that this heap handles.
         * For example: for RTV heap it will only be RTV descriptor type,
         * for DSV - DSV, for CBV/UAV/SRV heap it will be CBV, UAV and SRV (3 types).
         *
         * @return Descriptor types that this heap handles.
         */
        std::vector<DescriptorType> getDescriptorTypesHandledByThisHeap() const;

        /**
         * Recreates views for created descriptors
         * to be binded to the current heap.
         */
        void recreateOldViews() const;

    private:
        /** Do not delete. Owner renderer. */
        DirectXRenderer* pRenderer;

        /** Descriptor heap. */
        ComPtr<ID3D12DescriptorHeap> pHeap;

        /** Type of the heap. */
        DescriptorHeapType heapType;

        /** String version of heap type (used for logging). */
        std::string sHeapType;

        /** Size of one descriptor. */
        UINT iDescriptorSize = 0;

        /** Current heap capacity. */
        std::atomic<INT> iHeapCapacity{0};

        /** Current heap size (actually used size). */
        std::atomic<INT> iHeapSize{0};

        /**
         * Index of the next free descriptor that can be used in @ref bindedResources.
         * Each created descriptor will fetch this value (to be used) and increment it.
         * Once this value is equal to @ref iHeapCapacity we will use @ref noLongerUsedDescriptorIndexes
         * to see if any old descriptors were released and no longer being used.
         */
        std::atomic<INT> iNextFreeHeapIndex{0};

        /** Indexes of descriptors that were created but no longer being used. */
        std::queue<INT> noLongerUsedDescriptorIndexes;

        /** Mutex for read/write operations heap and descriptors. */
        std::recursive_mutex mtxRwHeap;

        /**
         * Set of resources that use created descriptors
         * (size might not be equal to the actual heap size).
         *
         * Storing a raw pointer here because it's only used to update
         * view if the heap was recreated (no resource ownership). Once resource
         * is destroyed the descriptor will also be destroyed
         * and thus it will be removed from this set.
         */
        std::unordered_set<DirectXResource*> bindedResources;

        /** Direct3D type of this heap. */
        D3D12_DESCRIPTOR_HEAP_TYPE d3dHeapType;

        /** Number of descriptors to add to the heap when there is no more free space left. */
        static constexpr INT iHeapGrowSize = 200; // NOLINT: don't recreate heap too often

        /** Name of the category used for logging. */
        static inline const auto sDescriptorHeapLogCategory = "Descriptor Heap";
    };
} // namespace ne
