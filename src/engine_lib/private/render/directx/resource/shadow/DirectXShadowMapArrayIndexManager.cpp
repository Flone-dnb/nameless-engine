#include "DirectXShadowMapArrayIndexManager.h"

// Custom.
#include "render/general/resource/shadow/ShadowMapHandle.h"
#include "render/directx/resource/DirectXResource.h"
#include "render/directx/resource/DirectXResourceManager.h"
#include "render/Renderer.h"

namespace ne {

    DirectXShadowMapArrayIndexManager::DirectXShadowMapArrayIndexManager(
        Renderer* pRenderer, const std::string& sShaderArrayResourceName)
        : ShadowMapArrayIndexManager(pRenderer, sShaderArrayResourceName) {}

    DirectXShadowMapArrayIndexManager::~DirectXShadowMapArrayIndexManager() {
        std::scoped_lock guard(mtxRegisteredShadowMaps.first);

        // Make sure no shadow map is registered.
        if (!mtxRegisteredShadowMaps.second.empty()) [[unlikely]] {
            Error error(std::format(
                "\"{}\" index manager is being destroyed but there are still {} registered shadow map "
                "handle(s) alive",
                getShaderArrayResourceName(),
                mtxRegisteredShadowMaps.second.size()));
            error.showError();
            return; // don't throw in destructor
        }
    }

    std::variant<std::unique_ptr<DirectXShadowMapArrayIndexManager>, Error>
    DirectXShadowMapArrayIndexManager::create(
        Renderer* pRenderer,
        GpuResourceManager* pResourceManager,
        const std::string& sShaderArrayResourceName) {
        const auto pDirectXResourceManager = dynamic_cast<DirectXResourceManager*>(pResourceManager);
        if (pDirectXResourceManager == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource manager");
        }

        // Create new index manager.
        auto pIndexManager = std::unique_ptr<DirectXShadowMapArrayIndexManager>(
            new DirectXShadowMapArrayIndexManager(pRenderer, sShaderArrayResourceName));
        const auto pIndexManagerRaw = pIndexManager.get();

        // Get SRV heap.
        const auto pSrvHeap = pDirectXResourceManager->getCbvSrvUavHeap();

        // Allocate SRV range.
        auto rangeResult = pSrvHeap->allocateContinuousDescriptorRange(
            sShaderArrayResourceName, [pIndexManagerRaw]() { pIndexManagerRaw->onSrvRangeIndicesChanged(); });
        if (std::holds_alternative<Error>(rangeResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(rangeResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pSrvRange = std::get<std::unique_ptr<ContinuousDirectXDescriptorRange>>(std::move(rangeResult));

        // Save SRV range.
        pIndexManager->pSrvRange = std::move(pSrvRange);

        return pIndexManager;
    }

    std::optional<Error>
    DirectXShadowMapArrayIndexManager::registerShadowMapResource(ShadowMapHandle* pShadowMapHandle) {
        // Lock resources.
        const auto pMtxResources = pShadowMapHandle->getResources();
        std::scoped_lock guard(pMtxResources->first, mtxRegisteredShadowMaps.first);

        // Convert resources.
        const auto pDepthTexture = dynamic_cast<DirectXResource*>(pMtxResources->second.pDepthTexture);
        if (pDepthTexture == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }
        const auto pColorTexture = reinterpret_cast<DirectXResource*>(pMtxResources->second.pColorTexture);

        // Bind a single DSV from descriptor heap (not a range).
        auto optionalError = pDepthTexture->bindDescriptor(DirectXDescriptorType::DSV);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        if (pColorTexture != nullptr) {
            // This is a shadow map handle for point light.
            // We also need a RTV to color texture during shadow pass for point lights.
            optionalError = pColorTexture->bindDescriptor(DirectXDescriptorType::RTV);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Self check: make sure this resource was not registered yet.
        if (mtxRegisteredShadowMaps.second.find(pShadowMapHandle) != mtxRegisteredShadowMaps.second.end())
            [[unlikely]] {
            return Error(std::format(
                "\"{}\" was requested to register a shadow map handle \"{}\" but this shadow map handle was "
                "already registered",
                getShaderArrayResourceName(),
                pDepthTexture->getResourceName()));
        }

        // Determine which resource to use to bind SRV.
        auto pSrvResource = pDepthTexture;
        if (pColorTexture != nullptr) {
            // This is a shadow map handle for point light.
            // Pixel shaders will sample from the color texture (not from the depth texture).
            pSrvResource = pColorTexture;
        }

        // Self check: make sure the resource does not have SRV yet.
        if (pSrvResource->getDescriptor(DirectXDescriptorType::SRV) != nullptr) [[unlikely]] {
            return Error(std::format(
                "\"{}\" was requested to register a shadow map handle \"{}\" but the GPU resource of this "
                "shadow map handle already has an SRV binded to it which is unexpected",
                getShaderArrayResourceName(),
                pSrvResource->getResourceName()));
        }

        // Bind SRV from the range.
        optionalError = pSrvResource->bindDescriptor(DirectXDescriptorType::SRV, pSrvRange.get(), false);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Get descriptor offset from range start.
        auto descriptorOffsetResult = pSrvRange->getResourceDescriptorOffsetFromRangeStart(
            reinterpret_cast<DirectXResource*>(pSrvResource), DirectXDescriptorType::SRV);
        if (std::holds_alternative<Error>(descriptorOffsetResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(descriptorOffsetResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iDescriptorOffsetFromRangeStart = std::get<unsigned int>(descriptorOffsetResult);

        // Save resource to internal array.
        mtxRegisteredShadowMaps.second.insert(pShadowMapHandle);

        // Notify shadow map user about array index initialized.
        changeShadowMapArrayIndex(pShadowMapHandle, iDescriptorOffsetFromRangeStart);

        return {};
    }

    std::optional<Error>
    DirectXShadowMapArrayIndexManager::unregisterShadowMapResource(ShadowMapHandle* pShadowMapHandle) {
        std::scoped_lock guard(mtxRegisteredShadowMaps.first);

        // Make sure this shadow map was previously registered.
        const auto shadowMapIt = mtxRegisteredShadowMaps.second.find(pShadowMapHandle);
        if (shadowMapIt == mtxRegisteredShadowMaps.second.end()) [[unlikely]] {
            return Error(std::format(
                "\"{}\" index manager is unable to unregister the specified shadow map handle because it was "
                "not registered previously",
                getShaderArrayResourceName()));
        }

        // Remove handle.
        mtxRegisteredShadowMaps.second.erase(shadowMapIt);

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

    void DirectXShadowMapArrayIndexManager::onSrvRangeIndicesChanged() {
        std::scoped_lock guard(mtxRegisteredShadowMaps.first);

        for (const auto& pShadowMapHandle : mtxRegisteredShadowMaps.second) {
            // Get handle resources.
            const auto pMtxResources = pShadowMapHandle->getResources();

            // Determine which resource to use to bind SRV.
            auto pSrvResource = pMtxResources->second.pDepthTexture;
            if (pMtxResources->second.pColorTexture != nullptr) {
                pSrvResource = pMtxResources->second.pColorTexture;
            }

            // Get descriptor offset from range start.
            auto descriptorOffsetResult = pSrvRange->getResourceDescriptorOffsetFromRangeStart(
                reinterpret_cast<DirectXResource*>(pSrvResource), DirectXDescriptorType::SRV);
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
