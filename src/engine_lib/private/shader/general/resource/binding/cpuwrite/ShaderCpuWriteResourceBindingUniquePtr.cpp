#include "ShaderCpuWriteResourceBindingUniquePtr.h"

// Custom.
#include "shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingManager.h"

namespace ne {
    ShaderCpuWriteResourceBindingUniquePtr::ShaderCpuWriteResourceBindingUniquePtr(
        ShaderCpuWriteResourceBindingManager* pManager, ShaderCpuWriteResourceBinding* pResource) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

    ShaderCpuWriteResourceBindingUniquePtr::ShaderCpuWriteResourceBindingUniquePtr(
        ShaderCpuWriteResourceBindingUniquePtr&& other) noexcept {
        *this = std::move(other);
    }

    ShaderCpuWriteResourceBindingUniquePtr& ShaderCpuWriteResourceBindingUniquePtr::operator=(
        ShaderCpuWriteResourceBindingUniquePtr&& other) noexcept {
        if (this != &other) {
#if defined(DEBUG)
            static_assert(
                sizeof(ShaderCpuWriteResourceBindingUniquePtr) == 16, // NOLINT: current size
                "consider adding new fields to move operator");
#endif

            pResource = other.pResource;
            other.pResource = nullptr;

            pManager = other.pManager;
            other.pManager = nullptr;
        }

        return *this;
    }

    ShaderCpuWriteResourceBindingUniquePtr::~ShaderCpuWriteResourceBindingUniquePtr() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->destroyResource(pResource);
    }

    void ShaderCpuWriteResourceBindingUniquePtr::markAsNeedsUpdate() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->markResourceAsNeedsUpdate(pResource);
    }

} // namespace ne
