#include "ShaderTextureResourceBindingUniquePtr.h"

// Custom.
#include "shader/general/resources/binding/texture/ShaderTextureResourceBindingManager.h"

namespace ne {
    ShaderTextureResourceBindingUniquePtr::ShaderTextureResourceBindingUniquePtr(
        ShaderTextureResourceBindingManager* pManager, ShaderTextureResourceBinding* pResource) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

    ShaderTextureResourceBindingUniquePtr::ShaderTextureResourceBindingUniquePtr(
        ShaderTextureResourceBindingUniquePtr&& other) noexcept {
        *this = std::move(other);
    }

    ShaderTextureResourceBindingUniquePtr&
    ShaderTextureResourceBindingUniquePtr::operator=(ShaderTextureResourceBindingUniquePtr&& other) noexcept {
        if (this != &other) {
#if defined(DEBUG)
            static_assert(
                sizeof(ShaderTextureResourceBindingUniquePtr) == 16, // NOLINT: current size
                "consider adding new fields to move operator");
#endif

            pResource = other.pResource;
            other.pResource = nullptr;

            pManager = other.pManager;
            other.pManager = nullptr;
        }

        return *this;
    }

    ShaderTextureResourceBindingUniquePtr::~ShaderTextureResourceBindingUniquePtr() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->destroyResource(pResource);
    }

} // namespace ne
