﻿#pragma once

// Standard.
#include <variant>
#include <memory>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <queue>

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

    /** Represents a descriptor heap. */
    class DirectXDescriptorHeap {
        // Notifies the heap about descriptor being destroyed.
        friend class DirectXDescriptor;

    public:
        /** Internal heap resources. */
        struct InternalResources {
            /** Descriptor heap. */
            ComPtr<ID3D12DescriptorHeap> pHeap;

            /** Current heap capacity. */
            INT iHeapCapacity = 0;

            /** Current heap size (actually used size). */
            INT iHeapSize = 0;

            /**
             * Index of the next free descriptor that can be used. Each created descriptor will fetch this
             * value (to be used) and increment it.
             *
             * @remark Once this value is equal to @ref iHeapCapacity we will use @ref
             * noLongerUsedDescriptorIndices to see if any old descriptors were released and no longer being
             * used.
             */
            INT iNextFreeHeapIndex = 0;

            /** Indices of descriptors that were created but no longer being used. */
            std::queue<INT> noLongerUsedDescriptorIndices;

            /**
             * Set of descriptors that use this heap.
             *
             * @remark Storing a raw pointer here because it's only used to update view if the heap was
             * recreated (no resource ownership). Once resource is destroyed the descriptor will also be
             * destroyed and thus it will be removed from this set.
             */
            std::unordered_set<DirectXDescriptor*> bindedDescriptors;
        };

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
         * @remark You can use this function to assign a different descriptor to already created resource.
         * For example: create SRV resource using resource manager and use RTV heap to assign
         * a RTV descriptor to this resource so it will have 2 different descriptors.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        assignDescriptor(DirectXResource* pResource, DirectXDescriptorType descriptorType);

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
        inline ID3D12DescriptorHeap* getInternalHeap() const {
            return mtxInternalResources.second.pHeap.Get();
        }

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
         * Marks resource descriptor(s) as no longer being used
         * so they can be reused by some other resource.
         *
         * @remark Called from DirectXDescriptor destructor.
         *
         * @param pDescriptor Descriptor that is being destroyed.
         */
        void onDescriptorBeingDestroyed(DirectXDescriptor* pDescriptor);

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
            DirectXDescriptorType descriptorType) const;

        /**
         * Recreates the heap to expand its capacity to support @ref iHeapGrowSize more descriptors.
         *
         * @remark All internal values and old descriptors will be updated.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> expandHeap();

        /**
         * Recreates the heap to shrink its capacity to support @ref iHeapGrowSize less descriptors.
         *
         * @remark All internal values and old descriptors will be updated.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> shrinkHeap();

        /**
         * (Re)creates the heap with the specified capacity.
         *
         * @remark Only updates the heap resource, internal capacity and updates old descriptors
         * (if any), other internal values are not changed and should be corrected afterwards.
         *
         * @param iCapacity Size of the heap (in descriptors).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createHeap(INT iCapacity);

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
         * @remark Generally called in @ref createHeap.
         */
        void recreateOldViews();

    private:
        /** Do not delete. Owner renderer. */
        DirectXRenderer* pRenderer;

        /** Descriptor heap internal resources. */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;

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
