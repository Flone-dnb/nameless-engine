#include "GpuResource.h"

namespace ne {

    GpuResource::GpuResource(
        const std::string& sResourceName, unsigned int iElementSizeInBytes, unsigned int iElementCount)
        : iElementSizeInBytes(iElementSizeInBytes), iElementCount(iElementCount),
          sResourceName(sResourceName) {}

    std::string GpuResource::getResourceName() const { return sResourceName; }

    unsigned int GpuResource::getElementSizeInBytes() const { return iElementSizeInBytes; }

    unsigned int GpuResource::getElementCount() const { return iElementCount; }

}
