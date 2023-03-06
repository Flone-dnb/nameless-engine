#include "materials/ShaderReadWriteResourceUniquePtr.h"

// Custom.
#include "materials/ShaderReadWriteResourceManager.h"

namespace ne {
    ShaderCpuReadWriteResourceUniquePtr::ShaderCpuReadWriteResourceUniquePtr(
        ShaderCpuReadWriteResourceManager* pManager, ShaderCpuReadWriteResource* pResource) {
        this->pManager = pManager;
        this->pResource = pResource;
    }

    ShaderCpuReadWriteResourceUniquePtr::ShaderCpuReadWriteResourceUniquePtr(
        ShaderCpuReadWriteResourceUniquePtr&& other) noexcept {
        *this = std::move(other);
    }

    ShaderCpuReadWriteResourceUniquePtr&
    ShaderCpuReadWriteResourceUniquePtr::operator=(ShaderCpuReadWriteResourceUniquePtr&& other) noexcept {
        if (this != &other) {
            static_assert(
                sizeof(ShaderCpuReadWriteResourceUniquePtr) == 16, // NOLINT: current size
                "consider adding new fields to move operator");

            pResource = other.pResource;
            other.pResource = nullptr;

            pManager = other.pManager;
            other.pManager = nullptr;
        }

        return *this;
    }

    ShaderCpuReadWriteResourceUniquePtr::~ShaderCpuReadWriteResourceUniquePtr() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->destroyResource(pResource);
    }

    void ShaderCpuReadWriteResourceUniquePtr::markAsNeedsUpdate() {
        if (pResource == nullptr) {
            // Our data was moved to some other object.
            return;
        }

        pManager->markResourceAsNeedsUpdate(pResource);
    }

} // namespace ne
