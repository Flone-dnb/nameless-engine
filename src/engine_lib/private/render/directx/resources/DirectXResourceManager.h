#pragma once

// Standard.
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/general/resources/GpuResourceManager.h"

// External.
#include "directx/d3dx12.h"
#include "D3D12MemoryAllocator/include/D3D12MemAlloc.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXResource;
    class DirectXRenderer;
    class UploadBuffer;

    /** Controls resource creation and descriptors heaps. */
    class DirectXResourceManager : public GpuResourceManager {
    public:
        DirectXResourceManager() = delete;
        DirectXResourceManager(const DirectXResourceManager&) = delete;
        DirectXResourceManager& operator=(const DirectXResourceManager&) = delete;

        virtual ~DirectXResourceManager() override = default;

        /**
         * Converts texture resource format to DirectX format.
         *
         * @param format Format to convert.
         *
         * @return DirectX format.
         */
        static DXGI_FORMAT
        convertTextureResourceFormatToDxFormat(ShaderReadWriteTextureResourceFormat format);

        /**
         * Creates a new resource manager.
         *
         * @param pRenderer DirectX renderer.
         *
         * @return Error if something went wrong, otherwise created resource manager.
         */
        static std::variant<std::unique_ptr<DirectXResourceManager>, Error>
        create(DirectXRenderer* pRenderer);

        /**
         * Loads a texture from a DDS file in the GPU memory.
         *
         * @param sResourceName     Resource name, used for logging.
         * @param pathToTextureFile Path to the image file that stores texture data.
         *
         * @return Error if something went wrong, otherwise created GPU resource that stores texture data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> loadTextureFromDisk(
            const std::string& sResourceName, const std::filesystem::path& pathToTextureFile) override;

        /**
         * Creates a new GPU resource with available CPU write access (only CPU write not read),
         * typically used for resources that needs to be frequently updated from the CPU side.
         *
         * Example:
         * @code
         * struct ObjectData{
         *     glm::mat4x4 world;
         * };
         *
         * auto result = pResourceManager->createResourceWithCpuWriteAccess(
         *     "object constant data",
         *     sizeof(ObjectData),
         *     1,
         *     false);
         * @endcode
         *
         * @remark This resource can be used as a `cbuffer` in shaders if `bIsUsedInShadersAsReadOnlyData`
         * is `true`.
         *
         * @param sResourceName                  Resource name, used for logging.
         * @param iElementSizeInBytes            Size of one buffer element in bytes.
         * @param iElementCount                  Number of elements in the resulting buffer.
         * @param isUsedInShadersAsArrayResource Specify `empty` if this resource is not going to be
         * used in shaders, `false` if this resource will be used in shaders as a single (non-array)
         * resource (cbuffer, uniform, might cause padding to 256 bytes and size limitation up to 64 KB) and
         * `true` if this resource will be used in shaders as an array resource (StructuredBuffer, storage
         * buffer).
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuWriteAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            std::optional<bool> isUsedInShadersAsArrayResource) override;

        /**
         * Creates a new GPU resource (buffer, not a texture) and fills it with the specified data.
         *
         * Example:
         * @code
         * std::vector<glm::vec3> vVertices;
         *
         * auto result = pResourceManager->createResourceWithData(
         *     "mesh vertex buffer",
         *     vVertices.data(),
         *     sizeof(glm::vec3),
         *     vVertices.size(),
         *     true);
         * @endcode
         *
         * @param sResourceName              Resource name, used for logging.
         * @param pBufferData                Pointer to the data that the new resource will contain.
         * @param iElementSizeInBytes        Size of one buffer element in bytes.
         * @param iElementCount              Number of elements in the resulting buffer.
         * @param usage                      Describes how you plan to use this resource.
         * @param bIsShaderReadWriteResource Specify `true` if you plan to modify the resource
         * from shaders, otherwise `false`.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResourceWithData(
            const std::string& sResourceName,
            const void* pBufferData,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            ResourceUsageType usage,
            bool bIsShaderReadWriteResource) override;

        /**
         * Creates a new GPU resource (buffer, not a texture) without any initial data.
         *
         * @remark This function can be useful if you plan to create a resource to be filled
         * from a (compute) shader and then use this data in some other shader.
         *
         * @param sResourceName              Resource name, used for logging.
         * @param iElementSizeInBytes        Size of one buffer element in bytes.
         * @param iElementCount              Number of elements in the resulting buffer.
         * @param usage                      Describes how you plan to use this resource.
         * @param bIsShaderReadWriteResource Specify `true` if you plan to modify the resource
         * from shaders, otherwise `false`.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResource(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            ResourceUsageType usage,
            bool bIsShaderReadWriteResource) override;

        /**
         * Creates a texture resource that is available as a read/write resource in shaders.
         *
         * @param sResourceName Resource name, used for logging.
         * @param iWidth        Width of the texture in pixels.
         * @param iHeight       Height of the texture in pixels.
         * @param format        Format of the texture.
         *
         * @return Error if something went wrong, otherwise created texture resource.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createShaderReadWriteTextureResource(
            const std::string& sResourceName,
            unsigned int iWidth,
            unsigned int iHeight,
            ShaderReadWriteTextureResourceFormat format) override;

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const override;

        /**
         * Returns the amount of video memory (VRAM) occupied by all currently allocated resources.
         *
         * @return Size of the video memory used by allocated resources.
         */
        virtual size_t getUsedVideoMemoryInMb() const override;

        /**
         * Creates a new GPU resource.
         *
         * @param sResourceName        Resource name, used for logging.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial resource state.
         * @param resourceClearValue   Clear value for render target or depth/stencil resources.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<DirectXResource>, Error> createResource(
            const std::string& sResourceName,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState,
            const std::optional<D3D12_CLEAR_VALUE>& resourceClearValue) const;

        /**
         * Creates new resources by wrapping swap chain buffers and binding
         * a RTV descriptor to each buffer.
         *
         * @param pSwapChain Swap chain.
         * @param iSwapChainBufferCount Number of buffers swap chain has.
         *
         * @return Error if something went wrong, otherwise created resources.
         */
        std::variant<std::vector<std::unique_ptr<DirectXResource>>, Error>
        makeRtvResourcesFromSwapChainBuffer(IDXGISwapChain3* pSwapChain, unsigned int iSwapChainBufferCount);

        /**
         * Returns RTV heap.
         *
         * @return RTV heap. Do not delete returned pointer.
         */
        DirectXDescriptorHeap* getRtvHeap() const;

        /**
         * Returns DSV heap.
         *
         * @return DSV heap. Do not delete returned pointer.
         */
        DirectXDescriptorHeap* getDsvHeap() const;

        /**
         * Returns CBV/SRV/UAV heap.
         *
         * @return CBV/SRV/UAV heap. Do not delete returned pointer.
         */
        DirectXDescriptorHeap* getCbvSrvUavHeap() const;

        /**
         * Dumps internal state of the resource manager in JSON format.
         *
         * @return JSON string.
         */
        virtual std::string getCurrentStateInfo() override;

    private:
        /**
         * Constructor.
         *
         * @param pRenderer        DirectX renderer.
         * @param pMemoryAllocator Created memory allocator to use.
         * @param pRtvHeap         Created RTV heap manager.
         * @param pDsvHeap         Created DSV heap manager.
         * @param pCbvSrvUavHeap   Created CBV/SRV/UAV heap.
         */
        DirectXResourceManager(
            DirectXRenderer* pRenderer,
            ComPtr<D3D12MA::Allocator>&& pMemoryAllocator,
            std::unique_ptr<DirectXDescriptorHeap>&& pRtvHeap,
            std::unique_ptr<DirectXDescriptorHeap>&& pDsvHeap,
            std::unique_ptr<DirectXDescriptorHeap>&& pCbvSrvUavHeap);

        /**
         * Modifies the value to be a multiple of 256.
         *
         * Example:
         * @code
         * const auto iResult = makeMultipleOf256(300);
         * assert(iResult == 512);
         * @endcode
         *
         * @param iNumber Value to make a multiple of 256.
         *
         * @return Resulting value.
         */
        static inline size_t makeMultipleOf256(size_t iNumber) {
            // We are adding 255 and then masking off
            // the lower 2 bytes which store all bits < 256.

            // Example: Suppose iNumber = 300.
            // (300 + 255) & ~255
            // 555 & ~255
            // 0x022B & ~0x00ff
            // 0x022B & 0xff00
            // 0x0200
            // 512
            return (iNumber + 255) & ~255; // NOLINT
        }

        /**
         * Creates a GPU resource to be used as a shadow map.
         *
         * @param sResourceName  Resource name, used for logging.
         * @param iTextureSize   Size of one dimension of the texture in pixels.
         * Must be power of 2 (128, 256, 512, 1024, 2048, etc.).
         * @param bIsCubeTexture `false` is you need a single 2D texture resource or `true` to have
         * 6 2D textures arranged as a cube map.
         *
         * @return Error if something went wrong, otherwise created texture resource.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createShadowMapTexture(
            const std::string& sResourceName, unsigned int iTextureSize, bool bIsCubeTexture) override;

        /**
         * Creates a new GPU resource and fills it with the specified data.
         *
         * @param sResourceName              Resource name, used for logging.
         * @param finalResourceDescription   Description of the final resource to create.
         * @param vSubresourcesToCopy        Describes the data that the resulting resource should have.
         * @param uploadResourceDescription  Description of the upload/staging resource.
         * @param bIsTextureResource         `true` if the final resource will be used as a read-only
         * texture in pixel shader, `false` if the final resource is not a texture.
         * @param iElementSizeInBytes        Optional size of one buffer element in bytes.
         * @param iElementCount              Optional number of elements in the resulting buffer.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        std::variant<std::unique_ptr<GpuResource>, Error> createResourceWithData(
            const std::string& sResourceName,
            const D3D12_RESOURCE_DESC& finalResourceDescription,
            const std::vector<D3D12_SUBRESOURCE_DATA>& vSubresourcesToCopy,
            const D3D12_RESOURCE_DESC& uploadResourceDescription,
            bool bIsTextureResource,
            size_t iElementSizeInBytes = 0,
            size_t iElementCount = 0);

        /** Allocator for GPU resources. */
        ComPtr<D3D12MA::Allocator> pMemoryAllocator;

        /** RTV heap manager. */
        std::unique_ptr<DirectXDescriptorHeap> pRtvHeap;

        /** DSV heap manager. */
        std::unique_ptr<DirectXDescriptorHeap> pDsvHeap;

        /** CBV/SRV/UAV heap manager. */
        std::unique_ptr<DirectXDescriptorHeap> pCbvSrvUavHeap;
    };
} // namespace ne
