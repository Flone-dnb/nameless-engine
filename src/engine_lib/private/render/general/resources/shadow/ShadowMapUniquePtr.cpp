#include "ShadowMapUniquePtr.h"

// Custom.
#include "render/general/resources/shadow/ShadowMapManager.h"

namespace ne {

    ShadowMapUniquePtr::~ShadowMapUniquePtr() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->destroyResource(pResource);
    }

    ShadowMapUniquePtr::ShadowMapUniquePtr(ShadowMapUniquePtr&& other) noexcept { *this = std::move(other); }

    ShadowMapUniquePtr& ShadowMapUniquePtr::operator=(ShadowMapUniquePtr&& other) noexcept {
        if (this != &other) {
#if defined(DEBUG)
            static_assert(
                sizeof(ShadowMapUniquePtr) == 16, // NOLINT: current size
                "consider adding new fields to move operator");
#endif

            pResource = other.pResource;
            other.pResource = nullptr;

            pManager = other.pManager;
            other.pManager = nullptr;
        }

        return *this;
    }

    ShadowMapUniquePtr::ShadowMapUniquePtr(ShadowMapManager* pManager, GpuResource* pResource) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

}
