#include "ShaderTextureResourceUniquePtr.h"

// Custom.
#include "material/resources/texture/ShaderTextureResourceManager.h"

namespace ne {
    ShaderTextureResourceUniquePtr::ShaderTextureResourceUniquePtr(
        ShaderTextureResourceManager* pManager, ShaderTextureResource* pResource) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

    ShaderTextureResourceUniquePtr::ShaderTextureResourceUniquePtr(
        ShaderTextureResourceUniquePtr&& other) noexcept {
        *this = std::move(other);
    }

    ShaderTextureResourceUniquePtr&
    ShaderTextureResourceUniquePtr::operator=(ShaderTextureResourceUniquePtr&& other) noexcept {
        if (this != &other) {
#if defined(DEBUG)
            static_assert(
                sizeof(ShaderTextureResourceUniquePtr) == 16, // NOLINT: current size
                "consider adding new fields to move operator");
#endif

            pResource = other.pResource;
            other.pResource = nullptr;

            pManager = other.pManager;
            other.pManager = nullptr;
        }

        return *this;
    }

    ShaderTextureResourceUniquePtr::~ShaderTextureResourceUniquePtr() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->destroyResource(pResource);
    }

} // namespace ne
