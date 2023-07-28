#include "ShaderResource.h"

// Custom.
#include "render/general/resources/GpuResource.h"

namespace ne {

    ShaderResourceBase::ShaderResourceBase(const std::string& sResourceName) {
        this->sResourceName = sResourceName;
    }

    std::string ShaderResourceBase::getResourceName() const { return sResourceName; }

    ShaderResource::~ShaderResource() {}

    ShaderResource::ShaderResource(
        const std::string& sResourceName, std::unique_ptr<GpuResource> pResourceData)
        : ShaderResourceBase(sResourceName) {
        this->pResourceData = std::move(pResourceData);
    }

    ShaderCpuWriteResource::~ShaderCpuWriteResource() {}

    void ShaderCpuWriteResource::markAsNeedsUpdate() {
        iFrameResourceCountToUpdate.store(FrameResourcesManager::getFrameResourcesCount());
    }

    ShaderCpuWriteResource::ShaderCpuWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderResourceBase(sResourceName) {
        this->iOriginalResourceSizeInBytes = iOriginalResourceSizeInBytes;
        this->vResourceData = std::move(vResourceData);
        this->onStartedUpdatingResource = onStartedUpdatingResource;
        this->onFinishedUpdatingResource = onFinishedUpdatingResource;
    }

} // namespace ne
