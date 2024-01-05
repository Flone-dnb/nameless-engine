#include "ShadowMapHandle.h"

// Custom.
#include "render/general/resources/shadow/ShadowMapManager.h"

namespace ne {

    ShadowMapHandle::~ShadowMapHandle() {
        std::scoped_lock guard(mtxResource.first);

        if (mtxResource.second == nullptr) [[unlikely]] {
            // Unexpected.
            Error error("shadow map handle has `nullptr` resource pointer");
            error.showError();
            return; // don't throw in destructor
        }

        // Notify manager.
        pManager->onShadowMapHandleBeingDestroyed(this);
    }

    ShadowMapHandle::ShadowMapHandle(
        ShadowMapManager* pManager,
        GpuResource* pResource,
        ShadowMapType type,
        size_t iTextureSize,
        const std::function<void(unsigned int)>& onArrayIndexChanged)
        : onArrayIndexChanged(onArrayIndexChanged), shadowMapType(type) {
        // Save manager.
        this->pManager = pManager;

        // Just in case check for `nullptr`.
        if (pResource == nullptr) [[unlikely]] {
            // Unexpected.
            Error error("unexpected `nullptr` resource pointer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save resource and size.
        mtxResource.second = pResource;
        iShadowMapSize = iTextureSize;
    }

    void ShadowMapHandle::changeArrayIndex(unsigned int iNewArrayIndex) {
        onArrayIndexChanged(iNewArrayIndex);
    }

}
