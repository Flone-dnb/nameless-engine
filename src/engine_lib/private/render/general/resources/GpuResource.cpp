#include "GpuResource.h"

namespace ne {

    GpuResource::GpuResource(unsigned int iElementSizeInBytes, unsigned int iElementCount)
        : iElementSizeInBytes(iElementSizeInBytes), iElementCount(iElementCount) {}

    unsigned int GpuResource::getElementSizeInBytes() const { return iElementSizeInBytes; }

    unsigned int GpuResource::getElementCount() const { return iElementCount; }

}
