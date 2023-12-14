#include "ShadowMapHandle.h"

// Custom.
#include "render/general/resources/shadow/ShadowMapManager.h"

namespace ne {

    ShadowMapHandle::~ShadowMapHandle() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        // Notify manager.
        pManager->destroyResource(this);

        // Clear resource pointer.
        pResource = nullptr;
    }

    ShadowMapHandle::ShadowMapHandle(ShadowMapManager* pManager, GpuResource* pResource, ShadowMapType type)
        : shadowMapType(type) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

}
