#include "ShaderResource.h"

// Custom.
#include "render/general/resources/GpuResource.h"

namespace ne {

    ShaderResource::ShaderResource(const std::string& sResourceName) { this->sResourceName = sResourceName; }

    std::string ShaderResource::getResourceName() const { return sResourceName; }

    ShaderCpuReadOnlyResource::~ShaderCpuReadOnlyResource() {}

    ShaderCpuReadOnlyResource::ShaderCpuReadOnlyResource(
        const std::string& sResourceName, std::unique_ptr<GpuResource> pResourceData)
        : ShaderResource(sResourceName) {
        this->pResourceData = std::move(pResourceData);
    }

    ShaderCpuReadWriteResource::~ShaderCpuReadWriteResource() {}

    void ShaderCpuReadWriteResource::markAsNeedsUpdate() {
        iFrameResourceCountToUpdate.store(FrameResourcesManager::getFrameResourcesCount());
    }

    ShaderCpuReadWriteResource::ShaderCpuReadWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderResource(sResourceName) {
        this->vResourceData = std::move(vResourceData);
        this->iOriginalResourceSizeInBytes = iOriginalResourceSizeInBytes;
        this->onStartedUpdatingResource = onStartedUpdatingResource;
        this->onFinishedUpdatingResource = onFinishedUpdatingResource;
    }

} // namespace ne
