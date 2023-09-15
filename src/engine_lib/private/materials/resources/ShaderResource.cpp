#include "ShaderResource.h"

namespace ne {

    ShaderResourceBase::ShaderResourceBase(const std::string& sResourceName, Pipeline* pUsedPipeline) {
        this->sResourceName = sResourceName;
        this->mtxCurrentPipeline.second = pUsedPipeline;
    }

    std::optional<Error> ShaderResourceBase::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxCurrentPipeline.first);

        // Call derived logic while pipeline is locked to avoid thread-races.
        auto optionalError = useNewPipeline(mtxCurrentPipeline.second);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::pair<std::recursive_mutex, Pipeline*>* ShaderResourceBase::getCurrentPipeline() {
        return &mtxCurrentPipeline;
    }

    std::optional<Error> ShaderResourceBase::useNewPipeline(Pipeline* pNewPipeline) {
        std::scoped_lock guard(mtxCurrentPipeline.first);

        // Update pipeline.
        mtxCurrentPipeline.second = pNewPipeline;

        // Call derived logic while pipeline is locked to avoid thread-races.
        auto optionalError = bindToNewPipeline(mtxCurrentPipeline.second);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::string ShaderResourceBase::getResourceName() const { return sResourceName; }

    std::optional<Error> ShaderTextureResource::useNewTexture(std::unique_ptr<TextureHandle> pTextureToUse) {
        // Get current pipeline.
        const auto pMtxCurrentPipeline = getCurrentPipeline();
        std::scoped_lock guard(pMtxCurrentPipeline->first);

        // Update descriptor.
        auto optionalError = updateTextureDescriptor(std::move(pTextureToUse), pMtxCurrentPipeline->second);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    ShaderTextureResource::ShaderTextureResource(const std::string& sResourceName, Pipeline* pUsedPipeline)
        : ShaderResourceBase(sResourceName, pUsedPipeline) {}

    ShaderCpuWriteResource::~ShaderCpuWriteResource() {}

    ShaderCpuWriteResource::ShaderCpuWriteResource(
        const std::string& sResourceName,
        Pipeline* pUsedPipeline,
        size_t iOriginalResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderResourceBase(sResourceName, pUsedPipeline) {
        this->iOriginalResourceSizeInBytes = iOriginalResourceSizeInBytes;
        this->onStartedUpdatingResource = onStartedUpdatingResource;
        this->onFinishedUpdatingResource = onFinishedUpdatingResource;
    }

} // namespace ne
