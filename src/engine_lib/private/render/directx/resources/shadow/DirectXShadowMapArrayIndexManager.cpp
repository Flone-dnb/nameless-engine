#include "DirectXShadowMapArrayIndexManager.h"

// Custom.
#include "render/general/resources/shadow/ShadowMapHandle.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/directx/resources/DirectXResourceManager.h"
#include "render/Renderer.h"

namespace ne {

    DirectXShadowMapArrayIndexManager::DirectXShadowMapArrayIndexManager(
        Renderer* pRenderer, const std::string& sArrayName)
        : ShadowMapArrayIndexManager(pRenderer, sArrayName) {}

    DirectXShadowMapArrayIndexManager::~DirectXShadowMapArrayIndexManager() {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure no shadow map is registered.
        if (!mtxInternalData.second.registeredShadowMaps.empty()) [[unlikely]] {
            Error error(std::format(
                "\"{}\" index manager is being destroyed but there are still {} registered shadow map "
                "handle(s) alive",
                getShaderArrayResourceName(),
                mtxInternalData.second.registeredShadowMaps.size()));
            error.showError();
            return; // don't throw in destructor
        }
    }

    std::variant<std::unique_ptr<DirectXShadowMapArrayIndexManager>, Error>
    DirectXShadowMapArrayIndexManager::create(Renderer* pRenderer, const std::string& sArrayName) {
        // Get resource manager.
        const auto pDirectXResourceManager =
            dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());
        if (pDirectXResourceManager == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource manager");
        }

        // Create new index manager.
        auto pIndexManager = std::unique_ptr<DirectXShadowMapArrayIndexManager>(
            new DirectXShadowMapArrayIndexManager(pRenderer, sArrayName));
        const auto pIndexManagerRaw = pIndexManager.get();

        // Get SRV heap.
        const auto pSrvHeap = pDirectXResourceManager->getCbvSrvUavHeap();

        // Allocate SRV range.
        auto rangeResult = pSrvHeap->allocateContinuousDescriptorRange(
            sArrayName, [pIndexManagerRaw]() { pIndexManagerRaw->onSrvRangeIndicesChanged(); });
        if (std::holds_alternative<Error>(rangeResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(rangeResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pSrvRange = std::get<std::unique_ptr<ContinuousDirectXDescriptorRange>>(std::move(rangeResult));

        // Save SRV range.
        pIndexManager->mtxInternalData.second.pSrvRange = std::move(pSrvRange);

        return pIndexManager;
    }

    std::optional<Error>
    DirectXShadowMapArrayIndexManager::registerShadowMap(ShadowMapHandle* pShadowMapHandle) {
        // Get resource.
        const auto pResource = dynamic_cast<DirectXResource*>(pShadowMapHandle->getResource());
        if (pResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Bind a single DSV from descriptor heap (not a range).
        auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::DSV);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        std::scoped_lock guard(mtxInternalData.first);

        // Self check: make sure this resource was not registered yet.
        if (mtxInternalData.second.registeredShadowMaps.find(pShadowMapHandle) !=
            mtxInternalData.second.registeredShadowMaps.end()) [[unlikely]] {
            return Error(std::format(
                "\"{}\" was requested to register a shadow map handle \"{}\" but this shadow map was already "
                "registered",
                getShaderArrayResourceName(),
                pShadowMapHandle->getResource()->getResourceName()));
        }

        // Get descriptor offset from range start.
        auto descriptorOffsetResult = getSrvDescriptorOffsetFromRangeStart(pResource);
        if (std::holds_alternative<Error>(descriptorOffsetResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(descriptorOffsetResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iDescriptorOffsetFromRangeStart = std::get<unsigned int>(descriptorOffsetResult);

        // Save resource to internal array.
        mtxInternalData.second.registeredShadowMaps.insert(pShadowMapHandle);

        // Notify shadow map user about array index initialized.
        changeShadowMapArrayIndex(pShadowMapHandle, iDescriptorOffsetFromRangeStart);

        return {};
    }

    std::optional<Error>
    DirectXShadowMapArrayIndexManager::unregisterShadowMap(ShadowMapHandle* pShadowMapHandle) {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure this shadow map was previously registered.
        const auto shadowMapIt = mtxInternalData.second.registeredShadowMaps.find(pShadowMapHandle);
        if (shadowMapIt == mtxInternalData.second.registeredShadowMaps.end()) [[unlikely]] {
            return Error(std::format(
                "\"{}\" index manager is unable to unregister the specified shadow map handle because it was "
                "not registered previously",
                getShaderArrayResourceName()));
        }

        // Remove handle.
        mtxInternalData.second.registeredShadowMaps.erase(shadowMapIt);

        // After this function is finished the resource is expected to be destroyed and thus descriptors
        // will be freed which will free some space in our SRV range.

        return {};
    }

    std::optional<Error> DirectXShadowMapArrayIndexManager::bindShadowMapsToPipeline(Pipeline* pPipeline) {
        // We will bind descriptor table when recording command list.
        return {};
    }

    std::optional<Error> DirectXShadowMapArrayIndexManager::bindShadowMapsToAllPipelines() {
        // We will bind descriptor table when recording command list.
        return {};
    }

    std::variant<unsigned int, Error>
    DirectXShadowMapArrayIndexManager::getSrvDescriptorOffsetFromRangeStart(DirectXResource* pResource) {
        // Get SRV descriptor.
        const auto pSrvDescriptor = pResource->getDescriptor(DirectXDescriptorType::SRV);
        if (pSrvDescriptor == nullptr) [[unlikely]] {
            return Error(std::format(
                "expected resource \"{}\" to have an SRV descriptor binded", pResource->getResourceName()));
        }

        std::scoped_lock guard(mtxInternalData.first);

        // Get descriptor offset from heap start.
        const auto iDescriptorOffsetFromHeapStart = pSrvDescriptor->getDescriptorOffsetInDescriptors();

        // Get range offset from heap start.
        const auto iRangeStartFromHeapStart = mtxInternalData.second.pSrvRange->getRangeStartInHeap();

        // Calculate offset from range start.
        const auto iDescriptorOffsetFromRangeStart =
            iDescriptorOffsetFromHeapStart - iRangeStartFromHeapStart;

        // Self check: make sure resulting value is non-negative.
        if (iDescriptorOffsetFromRangeStart < 0) [[unlikely]] {
            return Error(std::format(
                "\"{}\" index manager failed to calculate descriptor offset from range start (descriptor "
                "offset from heap: {}, range offset from heap: {}) because resulting descriptor offset from "
                "range start is negative: {})",
                getShaderArrayResourceName(),
                iDescriptorOffsetFromHeapStart,
                iRangeStartFromHeapStart,
                iDescriptorOffsetFromRangeStart));
        }

        return static_cast<unsigned int>(iDescriptorOffsetFromRangeStart);
    }

    void DirectXShadowMapArrayIndexManager::onSrvRangeIndicesChanged() {
        std::scoped_lock guard(mtxInternalData.first);

        for (const auto& pShadowMapHandle : mtxInternalData.second.registeredShadowMaps) {
            // Get descriptor offset from range start.
            auto descriptorOffsetResult = getSrvDescriptorOffsetFromRangeStart(
                reinterpret_cast<DirectXResource*>(pShadowMapHandle->getResource()));
            if (std::holds_alternative<Error>(descriptorOffsetResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(descriptorOffsetResult));
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            const auto iDescriptorOffsetFromRangeStart = std::get<unsigned int>(descriptorOffsetResult);

            // Notify shadow map user about array index initialized.
            changeShadowMapArrayIndex(pShadowMapHandle, iDescriptorOffsetFromRangeStart);
        }
    }

}
