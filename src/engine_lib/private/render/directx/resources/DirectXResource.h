#pragma once

// Standard.
#include <variant>
#include <memory>
#include <optional>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/GpuResource.h"
#include "render/directx/descriptors/DirectXDescriptor.h"

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

    /** D3D resource wrapper with automatic descriptor binding. */
    class DirectXResource : public GpuResource {
        // If descriptor heap of a used descriptor (see field of this class) was recreated
        // then descriptor heap will update our descriptors with new descriptor information.
        friend class DirectXDescriptorHeap;

    public:
        DirectXResource(const DirectXResource&) = delete;
        DirectXResource& operator=(const DirectXResource&) = delete;

        virtual ~DirectXResource() override = default;

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
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<DirectXResource>, Error> create(
            const DirectXResourceManager* pResourceManager,
            const std::string& sResourceName,
            D3D12MA::Allocator* pMemoryAllocator,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState,
            std::optional<D3D12_CLEAR_VALUE> resourceClearValue);

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
            const DirectXResourceManager* pResourceManager,
            DirectXDescriptorHeap* pRtvHeap,
            const ComPtr<ID3D12Resource>& pSwapChainBuffer);

        /**
         * Creates a new render target view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> addRtv();

        /**
         * Creates a new depth stencil view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> addDsv();

        /**
         * Creates a new constant buffer view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindCbv() override;

        /**
         * Creates a new shader resource view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> addSrv();

        /**
         * Creates a new unordered access view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> addUav();

        /**
         * Returns internal resource.
         *
         * @return Do not delete (free) this pointer. Internal resource.
         */
        ID3D12Resource* getInternalResource() const;

        /**
         * Returns resource name.
         *
         * @return Resource name.
         */
        std::string getResourceName() const;

    private:
        /**
         * Constructor. Creates an empty resource.
         *
         * @param pResourceManager Owner resource manager.
         */
        DirectXResource(const DirectXResourceManager* pResourceManager);

        /** Do not delete. Owner resource manager. */
        const DirectXResourceManager* pResourceManager = nullptr;

        /**
         * Will have size of DescriptorType enum elements.
         * Access elements like this: "vHeapDescriptors[DescriptorType::SRV]".
         */
        std::vector<std::optional<DirectXDescriptor>> vHeapDescriptors;

        /** Created resource (can be empty if @ref pSwapChainBuffer is used). */
        ComPtr<D3D12MA::Allocation> pAllocatedResource;

        /**
         * Used when resource was created from swap chain buffer
         * (can be empty if @ref pAllocatedResource is used).
         */
        ComPtr<ID3D12Resource> pSwapChainBuffer;
    };
} // namespace ne
