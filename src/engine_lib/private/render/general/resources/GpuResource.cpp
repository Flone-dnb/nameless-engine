#include "GpuResource.h"

// Custom.
#include "render/general/resources/GpuResourceManager.h"
#include "io/Logger.h"

namespace ne {

    GpuResource::GpuResource(
        GpuResourceManager* pManager,
        const std::string& sResourceName,
        unsigned int iElementSizeInBytes,
        unsigned int iElementCount)
        : pManager(pManager), iElementSizeInBytes(iElementSizeInBytes), iElementCount(iElementCount),
          sResourceName(sResourceName) {
        // Increment alive resource count.
        pManager->iAliveResourceCount.fetch_add(1);
    }

    GpuResource::~GpuResource() {
        // Decrement alive resource counter.
        const auto iPreviousTotalResourceCount = pManager->iAliveResourceCount.fetch_sub(1);

        // Self check: make sure the counter is not below zero.
        if (iPreviousTotalResourceCount == 0) [[unlikely]] {
            Logger::get().error("total alive GPU resource counter just went below zero");
        }
    }

    std::string GpuResource::getResourceName() const { return sResourceName; }

    unsigned int GpuResource::getElementSizeInBytes() const { return iElementSizeInBytes; }

    unsigned int GpuResource::getElementCount() const { return iElementCount; }

    GpuResourceManager* GpuResource::getResourceManager() const { return pManager; }

}
