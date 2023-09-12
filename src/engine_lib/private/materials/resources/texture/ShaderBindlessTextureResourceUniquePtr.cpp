#include "ShaderBindlessTextureResourceUniquePtr.h"

// Custom.
#include "materials/resources/texture/ShaderBindlessTextureResourceManager.h"

namespace ne {
    ShaderBindlessTextureResourceUniquePtr::ShaderBindlessTextureResourceUniquePtr(
        ShaderBindlessTextureResourceManager* pManager, ShaderBindlessTextureResource* pResource) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

    ShaderBindlessTextureResourceUniquePtr::ShaderBindlessTextureResourceUniquePtr(
        ShaderBindlessTextureResourceUniquePtr&& other) noexcept {
        *this = std::move(other);
    }

    ShaderBindlessTextureResourceUniquePtr& ShaderBindlessTextureResourceUniquePtr::operator=(
        ShaderBindlessTextureResourceUniquePtr&& other) noexcept {
        if (this != &other) {
#if defined(DEBUG)
            static_assert(
                sizeof(ShaderBindlessTextureResourceUniquePtr) == 16, // NOLINT: current size
                "consider adding new fields to move operator");
#endif

            pResource = other.pResource;
            other.pResource = nullptr;

            pManager = other.pManager;
            other.pManager = nullptr;
        }

        return *this;
    }

    ShaderBindlessTextureResourceUniquePtr::~ShaderBindlessTextureResourceUniquePtr() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->destroyResource(pResource);
    }

} // namespace ne
