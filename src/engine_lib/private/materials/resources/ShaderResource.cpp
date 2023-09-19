#include "ShaderResource.h"

namespace ne {

    ShaderResourceBase::ShaderResourceBase(const std::string& sResourceName) {
        this->sResourceName = sResourceName;
    }

    std::string ShaderResourceBase::getResourceName() const { return sResourceName; }

    ShaderTextureResource::ShaderTextureResource(const std::string& sResourceName)
        : ShaderResourceBase(sResourceName) {}

    ShaderCpuWriteResource::ShaderCpuWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderResourceBase(sResourceName) {
        this->iOriginalResourceSizeInBytes = iOriginalResourceSizeInBytes;
        this->onStartedUpdatingResource = onStartedUpdatingResource;
        this->onFinishedUpdatingResource = onFinishedUpdatingResource;
    }

} // namespace ne
