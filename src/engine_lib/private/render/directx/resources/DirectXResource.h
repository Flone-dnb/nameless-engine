#pragma once

// Standard.
#include <variant>
#include <memory>
#include <array>
#include <optional>
#include <mutex>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/GpuResource.h"
#include "render/directx/descriptors/DirectXDescriptor.h"
#include "render/directx/descriptors/DirectXDescriptorType.hpp"

// External.
#include "directx/d3dx12.h"
#include "D3D12MemoryAllocator/include/D3D12MemAlloc.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;
    class DirectXDescriptorHeap;
    class DirectXResourceManager;
    class ContinuousDirectXDescriptorRange;

    /** D3D resource wrapper with automatic descriptor binding. */
    class DirectXResource : public GpuResource {
        // If descriptor heap of a used descriptor (see field of this class) was recreated
        // then descriptor heap will update our descriptors with new descriptor information.
        friend class DirectXDescriptorHeap;

        // Only resource manager can create this resource
        // (simply because only manager has a memory allocator object).
        friend class DirectXResourceManager;

    public:
        DirectXResource() = delete;
        DirectXResource(const DirectXResource&) = delete;
        DirectXResource& operator=(const DirectXResource&) = delete;

        virtual ~DirectXResource() override;

        /**
         * Creates a new descriptor and binds it to this resource.
         *
         * @remark Does nothing if a descriptor of this type is already binded.
         *
         * @param descriptorType Type of descriptor to bind.
         * @param pRange         Specify in order to allocate a descriptor from this range.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindDescriptor(
            DirectXDescriptorType descriptorType, ContinuousDirectXDescriptorRange* pRange = nullptr);

        /**
         * Returns descriptor handle to the descriptor that was previously binded using @ref bindDescriptor.
         *
         * @param descriptorType Type of descriptor to get.
         *
         * @return Empty if descriptor if this type was not binded to this resource, otherwise
         * descriptor handle.
         */
        std::optional<D3D12_CPU_DESCRIPTOR_HANDLE>
        getBindedDescriptorCpuHandle(DirectXDescriptorType descriptorType);

        /**
         * Returns descriptor handle to the descriptor that was previously binded using @ref bindDescriptor.
         *
         * @param descriptorType Type of descriptor to get.
         *
         * @return Empty if descriptor if this type was not binded to this resource, otherwise
         * descriptor handle.
         */
        std::optional<D3D12_GPU_DESCRIPTOR_HANDLE>
        getBindedDescriptorGpuHandle(DirectXDescriptorType descriptorType);

        /**
         * Returns internal resource.
         *
         * @remark Do not delete (free) this pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return Internal resource.
         */
        inline ID3D12Resource* getInternalResource() const { return pInternalResource; }

        /**
         * Returns a raw (non-owning) pointer to a binded descriptor.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @param descriptorType Type of descriptor to query.
         *
         * @return `nullptr` if a descriptor of this type was not binded to this resource,
         * otherwise valid pointer.
         */
        DirectXDescriptor* getDescriptor(DirectXDescriptorType descriptorType);

    private:
        /**
         * Constructor. Creates an empty resource.
         *
         * @param pResourceManager     Owner resource manager.
         * @param sResourceName        Name of this resource.
         * @param iElementSizeInBytes  Resource size information. Size of one array element (if array),
         * otherwise specify size of the whole resource.
         * @param iElementCount        Resource size information. Total number of elements in the array (if
         * array), otherwise specify 1.
         */
        DirectXResource(
            DirectXResourceManager* pResourceManager,
            const std::string& sResourceName,
            UINT iElementSizeInBytes,
            UINT iElementCount);

        /**
         * Creates a new resource (without binding a descriptor to it).
         *
         * @param pResourceManager     Owner resource manager.
         * @param sResourceName        Resource name, used for logging.
         * @param pMemoryAllocator     Allocator to create resource.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial state of this resource.
         * @param resourceClearValue   Optimized clear value. Pass empty if creating
         * CBV/SRV/UAV resource.
         * @param iElementSizeInBytes  Optional parameter. Specify if this resource represents
         * an array. Used for SRV creation.
         * @param iElementCount        Optional parameter. Specify if this resource represents
         * an array. Used for SRV creation.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<DirectXResource>, Error> create(
            DirectXResourceManager* pResourceManager,
            const std::string& sResourceName,
            D3D12MA::Allocator* pMemoryAllocator,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState,
            std::optional<D3D12_CLEAR_VALUE> resourceClearValue,
            size_t iElementSizeInBytes = 0,
            size_t iElementCount = 0);

        /**
         * Creates a new resource instance by wrapping existing swap chain buffer,
         * also binds RTV to the specified resource.
         *
         * @param pResourceManager Owner resource manager.
         * @param pRtvHeap         Render target view heap manager.
         * @param pSwapChainBuffer Swap chain buffer to wrap.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<DirectXResource>, Error> createResourceFromSwapChainBuffer(
            DirectXResourceManager* pResourceManager,
            DirectXDescriptorHeap* pRtvHeap,
            const ComPtr<ID3D12Resource>& pSwapChainBuffer);

        /**
         * Array of descriptors used by this resource.
         *
         * @remark Access elements like this: "vHeapDescriptors[DirectXDescriptorType::SRV]".
         *
         * @remark Some descriptors are `nullptr`. `nullptr` descriptor means that it's not set (not used).
         */
        std::pair<
            std::recursive_mutex,
            std::array<std::unique_ptr<DirectXDescriptor>, static_cast<size_t>(DirectXDescriptorType::END)>>
            mtxHeapDescriptors;

        /** Created resource (can be empty if @ref pSwapChainBuffer is used). */
        ComPtr<D3D12MA::Allocation> pAllocatedResource;

        /**
         * Used when resource was created from swap chain buffer
         * (can be empty if @ref pAllocatedResource is used).
         */
        ComPtr<ID3D12Resource> pSwapChainBuffer;

        /**
         * Pointer to @ref pSwapChainBuffer or @ref pAllocatedResource used for fast access
         * to internal resource.
         */
        ID3D12Resource* pInternalResource = nullptr;
    };
} // namespace ne
