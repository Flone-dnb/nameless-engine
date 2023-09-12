#include "ShaderCpuWriteResourceUniquePtr.h"

// Custom.
#include "materials/resources/cpuwrite/ShaderCpuWriteResourceManager.h"

namespace ne {
    ShaderCpuWriteResourceUniquePtr::ShaderCpuWriteResourceUniquePtr(
        ShaderCpuWriteResourceManager* pManager, ShaderCpuWriteResource* pResource) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

    ShaderCpuWriteResourceUniquePtr::ShaderCpuWriteResourceUniquePtr(
        ShaderCpuWriteResourceUniquePtr&& other) noexcept {
        *this = std::move(other);
    }

    ShaderCpuWriteResourceUniquePtr&
    ShaderCpuWriteResourceUniquePtr::operator=(ShaderCpuWriteResourceUniquePtr&& other) noexcept {
        if (this != &other) {
#if defined(DEBUG)
            static_assert(
                sizeof(ShaderCpuWriteResourceUniquePtr) == 16, // NOLINT: current size
                "consider adding new fields to move operator");
#endif

            pResource = other.pResource;
            other.pResource = nullptr;

            pManager = other.pManager;
            other.pManager = nullptr;
        }

        return *this;
    }

    ShaderCpuWriteResourceUniquePtr::~ShaderCpuWriteResourceUniquePtr() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->destroyResource(pResource);
    }

    void ShaderCpuWriteResourceUniquePtr::markAsNeedsUpdate() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->markResourceAsNeedsUpdate(pResource);
    }

} // namespace ne
